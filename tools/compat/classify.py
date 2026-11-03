#!/usr/bin/env python3
"""classify.py <sweep dir> [--min-frames N] -- groups a playsweep run by what went wrong.

Reads each <pkg>.log in <sweep dir>, gives every app one verdict (works, opens, or the first fatal
signature in its log), and prints the signatures most apps share first: the fixes worth making first.
"""
import argparse, collections, glob, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from playsweep import summarise

def signature(log):
    """The first thing in a log that explains a failure, reduced to something shared across apps."""
    # Google Play's own protection (PairIP): a license check Husk can't pass, or its VM refusing to run. Not a Husk bug.
    # nothing to open: a tile, widget or keyboard with no launcher icon, on a phone too
    if 'the manifest names no launcher activity' in log:
        return 'not launchable: no launcher activity'
    if 'com.pairip.licensecheck.LicenseActivity' in log or 'libpairipcore.so' in log and '=== CRASH' in log:
        return 'protected: Play license check (PairIP)'
    for line in log.splitlines():
        m = re.search(r'bionic: exit\((\d+)\) called from (\S+\.so)', line)
        if m and m.group(1) != '0':
            return 'exit(%s) by the app\'s %s' % (m.group(1), re.sub(r'[\w]+_', '', m.group(2)) if 0 else m.group(2))
        m = re.search(r'=== CRASH: signal (\d+)', line)
        if m:
            lib = re.search(r'pc\s+\S+\s+(\S+\.so)', log)
            return 'crash signal %s%s' % (m.group(1), ' in ' + lib.group(1) if lib else '')
        m = re.search(r"main thread ended with ([\w.$/]+)(?::\s*(.*))?", line)
        if m:
            what = m.group(2) or ''
            what = re.sub(r'0x[0-9a-f]+|\d+', '#', what)[:70]
            return 'main thread ended: %s %s' % (m.group(1).split('/')[-1], what)
        m = re.search(r'No implementation found for ([\w/$.]+)', line)
        if m:
            return 'missing native method'
        if 'start failed' in line or 'could not be started' in line:
            return 'start failed: ' + line.strip()[:70]
        m = re.search(r'NoClassDefFoundError: ([\w/$.]+)', line)
        if m and not m.group(1).startswith(('vBH', 'com/google/android/gms')):
            return 'missing class %s' % '/'.join(m.group(1).split('/')[:3])
    return None

def drew_something(sweep_dir, pkg):
    """Whether the run's screenshot shows a screen rather than one flat colour: a menu-driven game (a crossword, a board game)
    draws a few frames and waits, so a frame count alone undercounts it. A flat 1748x804 PNG is under 25 KB; a web app's real
    content is in its .web.png."""
    sizes = [os.path.getsize(os.path.join(sweep_dir, pkg + ext)) for ext in ('.png', '.web.png') if os.path.exists(os.path.join(sweep_dir, pkg + ext))]
    return bool(sizes) and max(sizes) > 25000

def verdict(log, frames, sweep_dir, pkg, min_frames=30):
    sig = signature(log)
    if sig and not sig.startswith('missing class'):
        return sig
    if frames >= min_frames or (frames > 0 and drew_something(sweep_dir, pkg)):
        return 'works'
    # an anti-tamper layer linked into the engine (libunity needs libanort): its constructor waits forever, or its own code makes raw
    # system calls the kernel refuses. Free Fire, Vita Studio's and Oakever's games.
    if 'libanort.so' in log and not frames:
        return 'protected: libanort anti-tamper'
    return sig or ('opens (%d frames)' % frames if frames else 'no frames, no error')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('--min-frames', type=int, default=30, help='frames in the run that count as working')
    a = ap.parse_args()
    # from the logs, not summary.json: a summary written after a drive drop lists only the apps swept since
    verdicts = {}
    for path in sorted(glob.glob(os.path.join(a.dir, '*.log'))):
        pkg = os.path.basename(path)[:-4]
        if pkg.startswith('PlayDownload_'):
            continue
        log = open(path, errors='replace').read()
        r = summarise(log)
        r['rc'] = 0 if not r.get('crash') else 139
        v = verdict(log, r.get('frames') or 0, a.dir, pkg, a.min_frames)
        verdicts[pkg] = 'opens (few frames)' if v.startswith('opens') else v
    groups = collections.defaultdict(list)
    for pkg, v in verdicts.items():
        groups[v].append(pkg)
    for v, pkgs in sorted(groups.items(), key=lambda kv: (kv[0] != 'works', -len(kv[1]))):
        print('%4d  %s' % (len(pkgs), v))
        if v != 'works':
            print('        ' + ', '.join(sorted(pkgs)[:8]) + (' ...' if len(pkgs) > 8 else ''))

if __name__ == '__main__':
    main()
