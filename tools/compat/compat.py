#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Compatibility sweep: run every APK in a folder through the translation layer on this Mac and say how far each got.

    python3 tools/compat/compat.py <folder-of-apks> [--out DIR] [--secs N] [--only NAME ...]

For each APK it asks the app's own scan which engine the APK is (tools/compat/scan.c), runs it in that engine's Mac harness for a
while, and records whether it crashed, how many frames it drew, which missing libc functions it called and which Java methods it
reached that Husk does not implement, plus a screenshot. The result is <out>/report.html (a table to look through) and
<out>/summary.json (to diff between runs). What an APK needs next is in its row: that is the to-do list for compatibility.
"""
import argparse
import glob
import html
import json
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
TL = os.path.join(ROOT, 'src', 'translation-layer')

# Engine (as the scan names it) -> (harness source, portrait default)
HARNESS = {
    'Unity (IL2CPP)': 'tools/unity-test.c', 'Unity': 'tools/unity-test.c', 'Unity (Mono)': 'tools/unity-test.c',
    'Cocos': 'tools/cocos-test.c', 'Minecraft': 'tools/ga-test.c', 'SDL': 'tools/sdl-test.c', 'Python': 'tools/sdl-test.c',
    'Unreal Engine': 'tools/ue-test.c', 'NativeActivity': 'tools/ue-test.c', 'Rockstar': 'tools/gta-test.c',
    'Godot': 'tools/godot-test.c', 'Flutter': 'tools/flutter-test.c', 'GameMaker': 'tools/gamemaker-test.c',
}
WEB = {'Capacitor', 'Cordova'}


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def build_scan(work):
    out = os.path.join(work, 'tl-scan')
    if not os.path.exists(out):
        r = run(['clang', '-O1', '-I' + TL, os.path.join(ROOT, 'tools/compat/scan.c')]
                + [os.path.join(TL, f) for f in ('husk-tl-scan.c', 'husk-tl-zip.c', 'husk-tl-elf.c', 'husk-tl-json.c')]
                + ['-lz', '-o', out])
        if r.returncode:
            sys.exit('building the scan tool failed:\n' + r.stderr)
    return out


def build_harness(src, work):
    out = os.path.join(work, os.path.splitext(os.path.basename(src))[0])
    if not os.path.exists(out):
        print(f'  building {os.path.basename(out)}...', flush=True)
        r = run([os.path.join(ROOT, 'tools/regress/build.sh'), src, out])
        if r.returncode:
            print(r.stderr[-2000:])
            return None
    return out


def build_web(work):
    out = os.path.join(work, 'webapp-test')
    if not os.path.exists(out):
        r = run([os.path.join(ROOT, 'tools/webapp/build.sh'), out])
        if r.returncode:
            return None
    return out


def newest_frame(log_text):
    m = re.search(r'^frames: (\S+)', log_text, re.M)
    if not m or not os.path.isdir(m.group(1)):
        return None
    bmps = sorted(glob.glob(os.path.join(m.group(1), '*.bmp')), key=os.path.getmtime)
    return bmps[-1] if bmps else None


def analyse(log_text):
    crash = re.search(r'=== CRASH: [^\n]*\n\s*pc\s+(\S+)\s*(.*)', log_text)
    frames = re.findall(r'(\d+) frames? in \d+ s', log_text)
    unresolved = sorted(set(re.findall(r'CALLED an unresolved import: (\S+)', log_text)))
    missing_java = sorted(set(re.findall(r'jni: (?:Get\w*MethodID|Call\w*Method\w*)\(([^)]*\)[^)]*)\) -> exists, not implemented', log_text)))
    return {
        'crashed': bool(crash),
        'crash': (crash.group(0).splitlines()[0] + ' ' + crash.group(2).strip()) if crash else '',
        'frames': int(frames[-1]) if frames else None,
        'unresolved_called': unresolved,
        'java_not_implemented': missing_java[:60],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('folder')
    ap.add_argument('--out', default='/tmp/husk-compat')
    ap.add_argument('--secs', type=int, default=20)
    ap.add_argument('--only', nargs='*')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    work = os.path.join(a.out, '.bin')
    os.makedirs(work, exist_ok=True)
    scan = build_scan(work)
    apks = sorted(glob.glob(os.path.join(a.folder, '*.apk')))
    if a.only:
        apks = [p for p in apks if any(o in os.path.basename(p) for o in a.only)]
    results = []
    for apk in apks:
        name = os.path.splitext(os.path.basename(apk))[0]
        print(f'{name}', flush=True)
        rep = json.loads(run([scan, apk]).stdout or '{}')
        engine = rep.get('engine') or ''
        row = {'apk': os.path.basename(apk), 'engine': engine or '(none)', 'verdict': rep.get('verdict', '?')}
        log_path = os.path.join(a.out, name + '.log')
        shot = os.path.join(a.out, name + '.png')
        if os.path.exists(shot):
            os.remove(shot)
        t0 = time.time()
        if engine in WEB:
            exe = build_web(work)
            env = dict(os.environ, SHOT=shot)
            r = run([exe, apk, engine, name, str(min(a.secs, 10))], env=env, timeout=a.secs + 60) if exe else None
            text = (r.stdout + r.stderr) if r else 'web harness failed to build'
            row['status'] = 'runs' if 'probe:' in text and '"body":""' not in text else 'loads, empty page'
        elif engine in HARNESS:
            exe = build_harness(HARNESS[engine], work)
            if not exe:
                row['status'] = 'harness did not build'
                results.append(row)
                continue
            data = os.path.join(a.out, '.data', name)
            shutil.rmtree(data, ignore_errors=True)
            os.makedirs(data, exist_ok=True)
            env = dict(os.environ, TL_AUDIO_MUTE='1', TL_DATA=data, TL_PACKAGE=name, TL_PKG=name)
            try:
                r = run([exe, apk, str(a.secs), '1748', '804'], env=env, timeout=a.secs + 90)
                text = r.stdout + r.stderr
            except subprocess.TimeoutExpired as e:
                text = (e.stdout or b'').decode('utf-8', 'replace') if isinstance(e.stdout, bytes) else (e.stdout or '')
                text += '\n(timed out)'
            frame = newest_frame(text)
            if frame:
                run(['sips', '-s', 'format', 'png', '-Z', '480', frame, '--out', shot])
            info = analyse(text)
            row.update(info)
            if info['crashed']:
                row['status'] = 'crashes'
            elif info['frames']:
                row['status'] = 'runs'
            else:
                row['status'] = 'starts, no frames'
        else:
            row['status'] = 'no driver' if rep.get('verdict') != 'java' else 'Java only (needs the Java runtime)'
            text = json.dumps(rep, indent=1)
        with open(log_path, 'w') as f:
            f.write(text)
        row['seconds'] = round(time.time() - t0, 1)
        row['shot'] = os.path.basename(shot) if os.path.exists(shot) else ''
        print(f'  {row["engine"]}: {row["status"]}', flush=True)
        results.append(row)

    with open(os.path.join(a.out, 'summary.json'), 'w') as f:
        json.dump(results, f, indent=1)
    counts = {}
    for r in results:
        counts[r['status']] = counts.get(r['status'], 0) + 1
    rows = []
    for r in results:
        needs = ', '.join(r.get('unresolved_called', [])[:12])
        java = '<br>'.join(html.escape(j) for j in r.get('java_not_implemented', [])[:8])
        img = f'<img src="{html.escape(r["shot"])}">' if r.get('shot') else ''
        rows.append(f'<tr class="{html.escape(r["status"].split(",")[0].replace(" ", "-"))}"><td>{html.escape(r["apk"])}</td>'
                    f'<td>{html.escape(r["engine"])}</td><td><b>{html.escape(r["status"])}</b><br><small>{html.escape(r.get("crash", ""))}</small></td>'
                    f'<td>{r.get("frames") or ""}</td><td><small>{html.escape(needs)}</small></td><td><small>{java}</small></td><td>{img}</td></tr>')
    page = f"""<!doctype html><meta charset=utf-8><title>Husk compatibility</title>
<style>body{{font:14px -apple-system,system-ui;margin:24px;background:#fafafa;color:#111}}table{{border-collapse:collapse;width:100%}}
td,th{{border-bottom:1px solid #ddd;padding:8px;vertical-align:top;text-align:left}}img{{max-width:240px;border-radius:6px}}
tr.runs td:nth-child(3){{color:#167a36}}tr.crashes td:nth-child(3){{color:#b3261e}}small{{color:#555}}</style>
<h1>Husk compatibility sweep</h1><p>{len(results)} APKs: {', '.join(f'{v} {k}' for k, v in sorted(counts.items()))}</p>
<table><tr><th>APK</th><th>Engine</th><th>Result</th><th>Frames</th><th>Missing libc called</th><th>Java not implemented</th><th>Screen</th></tr>
{''.join(rows)}</table>"""
    with open(os.path.join(a.out, 'report.html'), 'w') as f:
        f.write(page)
    print(f'\n{os.path.join(a.out, "report.html")}')


if __name__ == '__main__':
    main()
