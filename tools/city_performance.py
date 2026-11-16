#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Summarize a confirmed in-world CSV window; never infer gameplay from login FPS."""
import argparse
import csv
import io
import json
import math
from pathlib import Path
import re
import statistics


def seconds(value):
    h, m, s = value.split(':')
    result = int(h) * 3600 + int(m) * 60 + float(s)
    if not (0 <= int(h) < 24 and 0 <= int(m) < 60 and 0 <= float(s) < 60):
        raise ValueError('Invalid local clock time')
    return result


def summarize(csv_text, log_text, start, end, expected_build=None):
    lo, hi = seconds(start), seconds(end)
    if hi < lo:
        raise ValueError('Use one same-day measurement window')
    rows = [r for r in csv.DictReader(io.StringIO(csv_text)) if lo <= seconds(r['time']) <= hi]
    if not rows:
        raise ValueError('No CSV intervals in the confirmed window')
    for row in rows:
        for key in ('fps', 'frames', 'frame_max_ms'):
            if not math.isfinite(float(row[key])) or float(row[key]) < 0:
                raise ValueError('Invalid performance measurement')
        if float(row['fps']) <= 0 or int(row['frames']) <= 0:
            raise ValueError('An interval has no measurable elapsed time')
    elapsed = sum(int(r['frames']) / float(r['fps']) for r in rows)
    frames = sum(int(r['frames']) for r in rows)
    metrics = {}
    for key in ('fps', 'tick_ms', 'render_ms', 'wait_ms', 'present_ms', 'draws', 'npcs', 'players'):
        if all(key in r for r in rows):
            values = [float(r[key]) for r in rows]
            if not all(math.isfinite(v) for v in values):
                raise ValueError('Non-finite interval statistic')
            metrics[key] = {'min': min(values), 'median': statistics.median(values), 'max': max(values)}
    build_match = re.search(r'\[boot\] build\s+: ([^\r\n]+)', log_text)
    build = build_match[1] if build_match else None
    if expected_build and (not build or expected_build not in build.split()):
        raise ValueError('Runtime log does not confirm the requested installed build')
    stages, memory, java, caps = [], [], [], []
    render_samples, tick_samples, frame_samples = [], [], []
    class_samples, class_by_key = [], {}
    clock = None
    for number, line in enumerate(log_text.splitlines(), 1):
        if '[prev]' in line:
            continue
        stamp = re.search(r'\[EC\.log\] \[(\d\d:\d\d:\d\d\.\d+)\]', line)
        if stamp:
            clock = stamp[1]
        if clock is None or not lo <= seconds(clock) <= hi:
            continue
        if '[FRAMECAP]' in line:
            caps.append({'time': clock, 'logLine': number, 'message': line.split('[FRAMECAP]', 1)[1].strip()})
        if '[ZX_WORSTFRAME]' in line:
            text = line.split('[ZX_WORSTFRAME]', 1)[1]
            values = dict((k, float(v)) for k, v in re.findall(r'([\w.-]+) (-?\d+(?:\.\d+)?)', text.split('|', 1)[0]))
            stages.append({'time': clock, 'logLine': number, 'worstFrameStageMs': values})
        if '[ZX_RENDERPROF]' in line:
            render_samples.append(_parse_renderprof(clock, number, line))
        if '[ZX_TICKPROF]' in line:
            tick_samples.append(_parse_tickprof(clock, number, line))
        if '[ZX_FRAMEPROF]' in line:
            frame_samples.append(_parse_frameprof(clock, number, line))
        match = re.search(r'memory-budget\[game-periodic\]: footprint=([\d.]+) MiB resident=([\d.]+) MiB available-before-jetsam=([\d.]+)', line)
        if match:
            memory.append({'nearTime': clock, 'footprintMiB': float(match[1]), 'residentMiB': float(match[2]), 'headroomMiB': float(match[3])})
        match = re.search(r'jvm-retained-partial\[game-periodic\]: objects=(\d+) headers=([\d.]+) MiB array-payload=([\d.]+) MiB', line)
        if match:
            java.append({'nearTime': clock, 'unfreedObjects': int(match[1]), 'headersMiB': float(match[2]), 'arrayPayloadMiB': float(match[3])})
        if 'jvm-class-allocations-partial[game-periodic]:' in line:
            entry = _parse_class_alloc_summary(clock, number, line)
            if entry:
                class_samples.append(entry)
                class_by_key[(entry['epoch'], entry['sample'])] = entry
        if 'jvm-class-allocation[game-periodic]:' in line:
            _append_class_alloc_row(class_by_key, line)
    trend = None
    if len(java) >= 2:
        span = seconds(java[-1]['nearTime']) - seconds(java[0]['nearTime'])
        if span > 0:
            trend = {'approxSeconds': span, 'objectDelta': java[-1]['unfreedObjects'] - java[0]['unfreedObjects'],
                     'approxObjectsPerSecond': (java[-1]['unfreedObjects'] - java[0]['unfreedObjects']) / span}
    return {'installedBuild': build, 'window': {'start': start, 'end': end, 'rows': len(rows), 'frames': frames,
            'inferredElapsedSecondsFromRoundedCSV': elapsed}, 'weightedFPS': frames / elapsed,
            'intervalsBelow30': sum(float(r['fps']) < 30 for r in rows),
            'intervalsBelow25': sum(float(r['fps']) < 25 for r in rows),
            'intervalsAtLeast50': sum(float(r['fps']) >= 50 for r in rows),
            'maxFrameMsFromCSV': max(float(r['frame_max_ms']) for r in rows),
            'intervalStatistics': metrics, 'worstFrameStages': stages,
            'renderProfSummary': _summarize_stage_dicts([s['stageMs'] for s in render_samples]),
            'renderProfDrawsSummary': _summarize_stage_dicts([s['totals'] for s in render_samples]),
            'tickProfSummary': _summarize_stage_dicts([s['stageMs'] for s in tick_samples]),
            'frameProfSummary': _summarize_stage_dicts([s['stageMs'] for s in frame_samples]),
            'memorySamples': memory, 'memoryTrend': _summarize_memory_trend(memory),
            'javaPartialSamples': java, 'javaCounterTrend': trend,
            'javaClassAllocationSamples': class_samples,
            'adaptiveFPSMessages': caps,
            'gpuTiming': 'unavailable', 'onDeviceSwitchResult': 'not established by FPS logs',
            'limits': ['CSV FPS is an interval average, not instantaneous minimum or 1% low.',
                       'Stage entries describe selected worst frames, not averages of all frames; stages can overlap.',
                       'The game stage named host is game logic, not Husk compositor work.',
                       'Unprefixed memory/Java entries use the preceding game clock for approximate alignment.',
                       'Java counters are partial requested storage, not reachability or the complete heap.',
                       'Footprint and process headroom are not total device free RAM.',
                       'Frame maxima from different clock/measurement paths must remain separate.',
                       'No conclusion about improvement without comparable route, camera, crowd, settings and thermal state.']}


def _parse_renderprof(clock, line_number, line):
    body = line.split('[ZX_RENDERPROF]', 1)[1]
    left, _, right = body.partition('|')
    left = left.split(':', 1)[-1]
    stage_ms = {k: float(v) for k, v, _ in re.findall(r'([a-z]+) (-?\d+(?:\.\d+)?)(?: \((\d+)\))?', left)}
    totals = {}
    if (d := re.search(r'\bdraws (\d+)', right)):
        totals['draws'] = float(d[1])
    if (t := re.search(r'\btris (\d+)K', right)):
        totals['trisK'] = float(t[1])
    return {'time': clock, 'logLine': line_number, 'stageMs': stage_ms, 'totals': totals}


def _parse_tickprof(clock, line_number, line):
    body = line.split('[ZX_TICKPROF]', 1)[1].split(':', 1)[-1]
    stripped = re.sub(r'\([^()]*\)', '', re.sub(r'\[[^\]]*\]', '', body))
    stage_ms = {k: float(v) for k, v in re.findall(r'([a-z]+) (-?\d+(?:\.\d+)?)', stripped)}
    return {'time': clock, 'logLine': line_number, 'stageMs': stage_ms}


def _parse_frameprof(clock, line_number, line):
    body = line.split('[ZX_FRAMEPROF]', 1)[1]
    stage_ms = {}
    if (f := re.search(r'\bframe (-?\d+(?:\.\d+)?) ms', body)):
        stage_ms['frame'] = float(f[1])
    rhs = body.split('=', 1)[-1].split('|', 1)[0]
    for k, v in re.findall(r'([\w-]+) (-?\d+(?:\.\d+)?)', rhs):
        stage_ms[k] = float(v)
    if (m := re.search(r'\bmax frame (\d+)', body)):
        stage_ms['maxFrame'] = float(m[1])
    return {'time': clock, 'logLine': line_number, 'stageMs': stage_ms}


def _summarize_stage_dicts(dicts):
    keys = []
    for d in dicts:
        for k in d:
            if k not in keys:
                keys.append(k)
    summary = {}
    for k in keys:
        vals = [d[k] for d in dicts if k in d]
        summary[k] = {'count': len(vals), 'min': min(vals), 'median': statistics.median(vals), 'max': max(vals)}
    return summary


def _summarize_memory_trend(memory):
    if len(memory) < 2:
        return None
    span = seconds(memory[-1]['nearTime']) - seconds(memory[0]['nearTime'])
    if span <= 0:
        return None
    return {'approxSeconds': span,
            'footprintDeltaMiB': memory[-1]['footprintMiB'] - memory[0]['footprintMiB'],
            'residentDeltaMiB': memory[-1]['residentMiB'] - memory[0]['residentMiB'],
            'headroomDeltaMiB': memory[-1]['headroomMiB'] - memory[0]['headroomMiB'],
            'maxFootprintMiB': max(m['footprintMiB'] for m in memory),
            'minHeadroomMiB': min(m['headroomMiB'] for m in memory)}


def _parse_kv_fields(text):
    return dict(re.findall(r'([\w-]+)=([^\s)]+)', text))


def _parse_class_alloc_summary(clock, line_number, line):
    kv = _parse_kv_fields(line.split('jvm-class-allocations-partial[game-periodic]:', 1)[1])
    if 'epoch' not in kv or 'sample' not in kv:
        return None
    return {'nearTime': clock, 'logLine': line_number, 'epoch': int(kv['epoch']), 'sample': int(kv['sample']),
            'baseline': int(kv.get('baseline', 0)) == 1, 'intervalSeconds': float(kv.get('interval', 0.0)),
            'groups': int(kv.get('groups', 0)), 'createdTotal': int(kv.get('created-total', 0)),
            'createdDelta': int(kv.get('created-delta', 0)),
            'arrayCreatedBytesDelta': int(kv.get('array-created-bytes-delta', 0)),
            'otherCreatedDelta': int(kv.get('other-created-delta', 0)),
            'otherArrayBytesDelta': int(kv.get('other-array-bytes-delta', 0)), 'topClasses': []}


def _append_class_alloc_row(by_key, line):
    kv = _parse_kv_fields(line.split('jvm-class-allocation[game-periodic]:', 1)[1])
    if 'epoch' not in kv or 'sample' not in kv or 'class' not in kv:
        return
    entry = by_key.get((int(kv['epoch']), int(kv['sample'])))
    if not entry:
        return
    entry['topClasses'].append({'declaration': int(kv.get('declaration', 0)), 'className': kv['class'],
        'createdTotal': int(kv.get('created-total', 0)), 'createdDelta': int(kv.get('created-delta', 0)),
        'headerCreatedBytesDelta': int(kv.get('header-created-bytes-delta', 0)),
        'arrayCreatedBytesDelta': int(kv.get('array-created-bytes-delta', 0))})


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--csv', type=Path, required=True)
    ap.add_argument('--log', type=Path, required=True)
    ap.add_argument('--start', required=True)
    ap.add_argument('--end', required=True)
    ap.add_argument('--build')
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    result = summarize(args.csv.read_text(encoding='utf8'), args.log.read_text(encoding='utf8', errors='replace'),
                       args.start, args.end, args.build)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: result[k] for k in ('weightedFPS', 'intervalsBelow30', 'maxFrameMsFromCSV', 'javaCounterTrend')}))


if __name__ == '__main__':
    main()
