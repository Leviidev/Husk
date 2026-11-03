#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Run apps as Google Play delivers them (a folder per package: <pkg>.apk and its splits, from tools/playdl) on this Mac, each in the
harness of the engine the app would choose, and record how far each got.

    python3 tools/compat/playsweep.py <folder-of-app-folders> [--out DIR] [--secs N] [--only PKG ...]

Writes <out>/<pkg>.log and <pkg>.png per app and <out>/summary.json; prints one line per app.
"""
import time, argparse, glob, json, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BIN = os.environ.get('HUSK_BIN', '/Volumes/GTAV/husk2/bin')
JAVA = os.environ.get('HUSK_JAVAAPP', '/Volumes/GTAV/husk2/java/javaapp-test')
SCAN = os.environ.get('HUSK_SCAN', '/Volumes/GTAV/husk2/tl-scan')
# the harness of the driver the app picks for each engine (TranslationLayer.swift's nativeEngine)
NATIVE = {'Unity (IL2CPP)': 'unity-test', 'Unity': 'unity-test', 'Unity (Mono)': 'unity-test',           'Cocos': 'cocos-test', 'Godot': 'godot-test', 'GameMaker': 'gamemaker-test',
          'NativeActivity': 'ue-test', 'Unreal Engine': 'ue-test', 'Rockstar': 'gta-test', 'Minecraft': 'ga-test'}
# harnesses that take a screenshot when told to (TL_CTL), rather than leaving frames in a folder

def apks(d):
    fs = sorted(glob.glob(os.path.join(d, '*.apk')), key=len)
    return fs[0], fs[1:]

def summarise(text):
    m = re.search(r"main thread ended with (.*)", text)
    crash = re.search(r'=== CRASH: ([^\n]*)\n\s*pc\s+\S+\s+(\S+)\s*(\S*)', text)
    frames = re.findall(r'(\d+) frames? in \d+ s', text) or re.findall(r'\(frame (\d+)\)', text)   # the Java harness says it on its screenshot
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
    ap.add_argument('--jitless', action='store_true', help="Husk 3.2's JIT-less mode: each app's libraries (and the Java runtime's) become "
                    'dylibs first (BIN/elf2macho-test --prepare, cached in --aot), and the app runs with no executable memory (TL_NO_XMEM=1)')
    ap.add_argument('--aot', default=os.path.expanduser('~/Library/Caches/husk-regress/aot'))
    ap.add_argument('--resume', action='store_true', help='skip apps that already have a log in --out, and add to its summary.json')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rows = []
    summary = os.path.join(a.out, 'summary.json')
    if a.resume and os.path.exists(summary): rows = json.load(open(summary))
    for d in sorted(glob.glob(os.path.join(a.folder, '*'))):
        if not os.path.isdir(d): continue
        pkg = os.path.basename(d)
        if a.only and pkg not in a.only: continue
        if a.resume and os.path.exists(os.path.join(a.out, pkg + '.log')): continue
        # another GPU job (a PS5 run) wants the GPU to itself: wait between apps while this file exists
        while os.path.exists('/tmp/aw/pause'): time.sleep(10)
        # the external drive the apps and harness live on drops out when it overheats: wait for it rather than fail every app
        while not os.path.isdir(os.path.dirname(a.out.rstrip('/'))) or not os.path.isdir(a.folder): time.sleep(30)
        if not glob.glob(os.path.join(d, '*.apk')): continue
        base, splits = apks(d)
        rep = json.loads(subprocess.run([SCAN, base] + splits, capture_output=True, text=True).stdout or '{}')
        engine = rep.get('engine') or ''
        log, shot = os.path.join(a.out, pkg + '.log'), os.path.join(a.out, pkg + '.png')
        for f in (log, shot, shot[:-4] + '.web.png'):
            if os.path.exists(f): os.remove(f)
        env = dict(os.environ, TL_AUDIO_MUTE='1', TL_PACKAGE=pkg, TL_PKG=pkg, TMPDIR=os.environ.get('HUSK_TMP', '/Volumes/GTAV/husk2/tmp'))
        if 'HUSK_TMP' in os.environ: env.setdefault('TL_TMP', os.environ['HUSK_TMP'])
        abis = rep.get('abis') or []
        if abis and 'arm64-v8a' not in abis:
            rows.append({'pkg': pkg, 'engine': engine, 'harness': None, 'verdict': 'no arm64 code'}); print(pkg, 'no arm64 code'); continue
        libs = [l['name'] for l in rep.get('libraries', []) if l.get('abi') == 'arm64-v8a']
        if a.jitless:
            runtime = glob.glob(os.path.join(ROOT, 'build', 'java-runtime', '**', '*.so'), recursive=True)
            p = subprocess.run([os.path.join(BIN, 'elf2macho-test'), '--prepare', a.aot, base] + splits + runtime, capture_output=True, text=True, errors='replace')
            if p.returncode != 0:
                rows.append({'pkg': pkg, 'engine': engine, 'harness': None, 'verdict': 'preparing dylibs failed: ' + (p.stdout + p.stderr)[-200:]})
                print(pkg, 'preparing dylibs failed'); continue
            env.update(TL_NO_XMEM='1', TL_AOT_DIR=a.aot)
        if engine == 'Cocos' and 'libfmod.so' not in libs: engine = 'Java'          # the app's rule: other cocos2d-x games run on Java
        if engine in NATIVE:
            harness = NATIVE[engine]
            env['TL_SPLITS'] = ':'.join(splits)
            cmd = [os.path.join(BIN, harness), base, str(a.secs + 20), '1748', '804']
            # a screenshot near the end of the run, through the harness's control file (every native harness reads one)
            ctl = os.path.join(a.out, pkg + '.ctl')
            open(ctl, 'w').write('wait %d\nshot %s\nquit\n' % (a.secs * 1000, shot))
            env['TL_CTL'] = ctl
        else:
            harness = 'javaapp-test'
            cmd = [JAVA, d, str(a.secs + 10)]
            # the screenshot through the control file too: it also saves a web view's own picture (<pkg>.web.png), which the
            # frames of the app's views do not show
            ctl = os.path.join(a.out, pkg + '.ctl')
            open(ctl, 'w').write('wait %d\nshot %s\nwait 3000\nquit\n' % (a.secs * 1000, shot))
            env['TL_CTL'] = ctl
        try:
            r = subprocess.run(cmd, env=env, capture_output=True, text=True, errors='replace', timeout=a.secs + 90, cwd='/')   # an app's working directory is / on a phone (Unity builds paths from it)
            text = r.stdout + r.stderr; rc = r.returncode
        except subprocess.TimeoutExpired as e:
            text = (e.stdout or b'').decode('utf-8', 'replace') if isinstance(e.stdout, bytes) else (e.stdout or ''); rc = 'timeout'
        if not os.path.isdir(a.folder) or not os.path.isdir(a.out):
            # the drive dropped during the run: its result means nothing, so wait for the drive and leave this app without a log
            # (a --resume pass runs it again)
            print('%-40s drive dropped during the run; skipped' % pkg, flush=True)
            while not os.path.isdir(a.folder): time.sleep(30)
            os.makedirs(a.out, exist_ok=True)
            continue
        open(log, 'w').write(text)
        fm = re.search(r'^frames: (\S+)', text, re.M)
        if fm:
            bmps = sorted(glob.glob(os.path.join(fm.group(1), '*.bmp')), key=os.path.getmtime)
            if bmps and not os.path.exists(shot): subprocess.run(['sips', '-s', 'format', 'png', bmps[-1], '--out', shot], capture_output=True)
            subprocess.run(['rm', '-rf', fm.group(1)])
        dm = re.search(r'^data: (\S+)', text, re.M)
        if dm and dm.group(1).startswith(os.environ.get('HUSK_TMP', '/Volumes/GTAV/husk2/tmp') + '/'): subprocess.run(['chmod', '-R', 'u+w', dm.group(1)], capture_output=True); subprocess.run(['rm', '-rf', dm.group(1)])
        row = {'pkg': pkg, 'engine': engine or 'Java', 'harness': harness, 'rc': rc}
        row.update(summarise(text))
        rows = [r for r in rows if r.get('pkg') != pkg] + [row]
        json.dump(rows, open(summary, 'w'), indent=1)
        print('%-40s %-16s rc=%-7s frames=%-5s %s' % (pkg, (engine or 'Java')[:16], rc, row['frames'], row['java_error'] or row['crash'] or row['exit'] or ('finished' if row['finished'] else '')), flush=True)
    json.dump(rows, open(os.path.join(a.out, 'summary.json'), 'w'), indent=1)

if __name__ == '__main__':   # classify.py and the pipelines import summarise() from here
    main()
