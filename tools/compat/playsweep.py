#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Run apps as Google Play delivers them (a folder per package: <pkg>.apk and its splits, from tools/playdl) on this Mac, each in the
harness of the engine the app would choose, and record how far each got.

    python3 tools/compat/playsweep.py <folder-of-app-folders> [--out DIR] [--secs N] [--only PKG ...]

Writes <out>/<pkg>.log and <pkg>.png per app and <out>/summary.json; prints one line per app.
"""
import argparse, glob, json, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BIN = '/Volumes/GTAV/husk2/bin'
JAVA = '/Volumes/GTAV/husk2/java/javaapp-test'
SCAN = '/Volumes/GTAV/husk2/tl-scan'
NATIVE = {'Unity (IL2CPP)': 'unity-test', 'Unity': 'unity-test', 'Unity (Mono)': 'unity-test', 'Flutter': 'flutter-test',
          'Cocos': 'cocos-test', 'SDL': 'sdl-test', 'Python': 'sdl-test'}

def apks(d):
    fs = sorted(glob.glob(os.path.join(d, '*.apk')), key=len)
    return fs[0], fs[1:]

def summarise(text):
    m = re.search(r"main thread ended with (.*)", text)
    crash = re.search(r'=== CRASH: ([^\n]*)\n\s*pc\s+\S+\s+(\S+)\s*(\S*)', text)
    frames = re.findall(r'(\d+) frames? in \d+ s', text)
    exitm = re.search(r'bionic: exit\((\d+)\) called from (\S+)', text)
    return {
        'java_error': m.group(1)[:300] if m else None,
        'crash': ('%s in %s %s' % (crash.group(1), crash.group(2), crash.group(3))) if crash else None,
        'exit': ('exit(%s) from %s' % exitm.groups()) if exitm else None,
        'finished': 'the app finished' in text,
        'frames': int(frames[-1]) if frames else None,
        'unresolved': sorted(set(re.findall(r'CALLED an unresolved import: (\S+)', text)))[:10],
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('folder'); ap.add_argument('--out', default='/Volumes/GTAV/husk2/sweep'); ap.add_argument('--secs', type=int, default=45)
    ap.add_argument('--only', nargs='*')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rows = []
    for d in sorted(glob.glob(os.path.join(a.folder, '*'))):
        if not os.path.isdir(d): continue
        pkg = os.path.basename(d)
        if a.only and pkg not in a.only: continue
        if not glob.glob(os.path.join(d, '*.apk')): continue
        base, splits = apks(d)
        rep = json.loads(subprocess.run([SCAN, base] + splits, capture_output=True, text=True).stdout or '{}')
        engine = rep.get('engine') or ''
        log, shot = os.path.join(a.out, pkg + '.log'), os.path.join(a.out, pkg + '.png')
        for f in (log, shot):
            if os.path.exists(f): os.remove(f)
        env = dict(os.environ, TL_AUDIO_MUTE='1', TL_PACKAGE=pkg, TL_PKG=pkg, TMPDIR='/Volumes/GTAV/husk2/tmp')
        abis = rep.get('abis') or []
        if abis and 'arm64-v8a' not in abis:
            rows.append({'pkg': pkg, 'engine': engine, 'harness': None, 'verdict': 'no arm64 code'}); print(pkg, 'no arm64 code'); continue
        if engine in NATIVE:
            harness = NATIVE[engine]
            env['TL_SPLITS'] = ':'.join(splits)
            cmd = [os.path.join(BIN, harness), base, str(a.secs), '1748', '804']
        else:
            harness = 'javaapp-test'
            cmd = [JAVA, d, str(a.secs)]
        try:
            r = subprocess.run(cmd, env=env, capture_output=True, text=True, errors='replace', timeout=a.secs + 90, cwd='/Volumes/GTAV/husk2/java')
            text = r.stdout + r.stderr; rc = r.returncode
        except subprocess.TimeoutExpired as e:
            text = (e.stdout or b'').decode('utf-8', 'replace') if isinstance(e.stdout, bytes) else (e.stdout or ''); rc = 'timeout'
        open(log, 'w').write(text)
        fm = re.search(r'^frames: (\S+)', text, re.M)
        if fm:
            bmps = sorted(glob.glob(os.path.join(fm.group(1), '*.bmp')), key=os.path.getmtime)
            if bmps: subprocess.run(['sips', '-s', 'format', 'png', bmps[-1], '--out', shot], capture_output=True)
            subprocess.run(['rm', '-rf', fm.group(1)])
        dm = re.search(r'^data: (\S+)', text, re.M)
        if dm and dm.group(1).startswith('/Volumes/GTAV/husk2/tmp/'): subprocess.run(['rm', '-rf', dm.group(1)])
        row = {'pkg': pkg, 'engine': engine or 'Java', 'harness': harness, 'rc': rc}
        row.update(summarise(text))
        rows.append(row)
        print('%-40s %-16s rc=%-7s frames=%-5s %s' % (pkg, (engine or 'Java')[:16], rc, row['frames'], row['java_error'] or row['crash'] or row['exit'] or ('finished' if row['finished'] else '')), flush=True)
    json.dump(rows, open(os.path.join(a.out, 'summary.json'), 'w'), indent=1)

main()
