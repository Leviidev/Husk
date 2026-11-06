#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read Husk logs over a trusted USB connection into device-logs/latest.

No IPA rebuild, developer tunnel, cloud upload or device file writes are needed.
Install the optional dependency with scripts/ios_logs.ps1 -Setup, then run that
launcher again to follow logs. --once takes one snapshot and exits.
"""
import argparse
import asyncio
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import os
from contextlib import contextmanager
from pathlib import Path, PurePosixPath
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
LOG_NAMES = ('husk.log', 'husk-prev.log', 'husk-native.log', 'husk-native-prev.log')
MAX_LOG_BYTES = 16 * 1024 * 1024
MAX_CRASH_BYTES = 8 * 1024 * 1024


def husk_bundle(bundle):
    return bundle == 'com.husk.app' or bundle.startswith('com.husk.app.')


def select_bundle(apps, requested=None):
    if requested and (not husk_bundle(requested) or requested not in apps):
        raise ValueError('The requested installed bundle must be Husk')
    matches = [bid for bid in apps if husk_bundle(bid)]
    if requested:
        return requested
    if len(matches) != 1:
        raise ValueError('Expected one Husk installation; specify --bundle-id if several are installed')
    return matches[0]


@dataclass
class Cursor:
    offset: int
    prefix: bytes
    identity: tuple


async def mirror_log(afc, remote, target, cursors, limit=MAX_LOG_BYTES):
    """Read only new bytes, detect replacement/truncation, and bound local disk use."""
    info = await afc.stat(remote)
    size = int(info['st_size'])
    if size < 0:
        raise ValueError('Invalid remote file size')
    identity = (str(info.get('st_ino', '')), str(info.get('st_birthtime', '')))
    handle = await afc.fopen(remote, 'r')
    try:
        prefix = await afc.fread(handle, min(256, size)) if size else b''
        old = cursors.get(remote)
        same_prefix = old and old.prefix[:len(prefix)] == prefix[:len(old.prefix)]
        reset = not old or old.identity != identity or size < old.offset or not same_prefix or not target.exists()
        offset = max(0, size-limit) if reset else max(old.offset, size-limit)
        # A large jump must replace the local tail rather than concatenate a gap.
        reset = reset or (old is not None and offset != old.offset)
        await afc.fseek(handle, offset)
        data = await afc.fread(handle, size-offset) if size > offset else b''
    finally:
        await afc.fclose(handle)
    target.parent.mkdir(parents=True, exist_ok=True)
    changed = reset or bool(data)
    if reset:
        target.write_bytes(data[-limit:])
    elif data:
        if target.stat().st_size + len(data) <= limit:
            with target.open('ab') as f:
                f.write(data)
        else:
            with target.open('rb') as f:
                f.seek(max(0, target.stat().st_size-(limit-len(data))))
                previous = f.read()
            target.write_bytes((previous+data)[-limit:])
    cursors[remote] = Cursor(offset+len(data), prefix, identity)
    return {'name': target.name, 'remoteBytes': size, 'localBytes': target.stat().st_size,
            'tailOnly': size > limit, 'changed': bool(changed)}


async def collect_crashes(device, output):
    """Copy only Husk reports and Jetsam snapshots mentioning Husk; never remove them."""
    from pymobiledevice3.services.crash_reports import CrashReportsManager
    output.mkdir(parents=True, exist_ok=True)
    saved = []
    async with CrashReportsManager(device) as manager:
        try:
            await asyncio.wait_for(manager.flush(), timeout=5)
        except (TimeoutError, OSError):
            pass
        names = await manager.ls()
        candidates = sorted((p for p in names if re.fullmatch(r'/?Husk-[^/\\]+\.ips', p)), reverse=True)[:8]
        candidates += sorted((p for p in names if re.fullmatch(r'/?JetsamEvent-[^/\\]+\.ips', p)), reverse=True)[:5]
        for remote in candidates:
            leaf = PurePosixPath(remote).name
            target = output / leaf
            if target.exists():
                saved.append(leaf)
                continue
            info = await manager.afc.stat(remote)
            size = int(info['st_size'])
            if not 0 < size <= MAX_CRASH_BYTES:
                continue
            handle = await manager.afc.fopen(remote, 'r')
            try:
                data = await manager.afc.fread(handle, size)
            finally:
                await manager.afc.fclose(handle)
            if len(data) != size:
                continue
            if leaf.startswith('JetsamEvent-') and b'Husk' not in data and b'com.husk.app' not in data:
                continue
            target.write_bytes(data)
            saved.append(leaf)
    # Bound reports retained locally, using only names this collector owns.
    kept = []
    for prefix, count in (('Husk', 8), ('JetsamEvent', 2)):
        owned = sorted((p for p in output.iterdir() if p.is_file() and
                        re.fullmatch(prefix+r'-[^/\\]+\.ips', p.name)), reverse=True)
        kept += owned[:count]
        for old in owned[count:]:
            old.unlink()
    return [p.name for p in kept]


def write_status(output, **fields):
    fields['collectorPID'] = os.getpid()
    fields['updatedUTC'] = datetime.now(timezone.utc).isoformat()
    temporary = output / 'status.json.tmp'
    temporary.write_text(json.dumps(fields, indent=2)+'\n', encoding='utf-8')
    temporary.replace(output / 'status.json')


@contextmanager
def capture_lock(output):
    """One writer per destination; the OS releases the lock if the collector exits."""
    output.mkdir(parents=True, exist_ok=True)
    with (output/'.capture.lock').open('a+b') as lock:
        if not lock.tell():
            lock.write(b'\0');lock.flush()
        lock.seek(0)
        try:
            if os.name == 'nt':
                import msvcrt
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            raise RuntimeError('A USB collector already owns this output folder') from None
        yield


async def capture(args):
    from pymobiledevice3.lockdown import create_using_usbmux
    from pymobiledevice3 import usbmux
    from pymobiledevice3.services.installation_proxy import InstallationProxyService
    from pymobiledevice3.services.house_arrest import HouseArrestService
    from pymobiledevice3.exceptions import AfcFileNotFoundError
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    deadline = asyncio.get_running_loop().time()+args.duration if args.duration else None
    while True:
        try:
            devices = await usbmux.list_devices()
            usb = [d for d in devices if d.connection_type == 'USB' and (not args.udid or d.serial == args.udid)]
            if len(usb) != 1:
                raise ValueError('Connect exactly one USB iPhone, or specify --udid')
            async with await create_using_usbmux(serial=usb[0].serial, connection_type='USB', autopair=False) as device:
                async with InstallationProxyService(device) as apps:
                    installed = await apps.get_apps(application_type='User')
                bundle = select_bundle(installed, args.bundle_id)
                print('USB connected; reading Husk logs into '+str(output), flush=True)
                cursors = {}
                next_crashes = 0
                crash_files = []
                async with await HouseArrestService.create(device, bundle, documents_only=True) as afc:
                    while True:
                        files = []
                        for name in LOG_NAMES:
                            try:
                                files.append(await mirror_log(afc, '/Documents/'+name, output/name, cursors))
                            except AfcFileNotFoundError:
                                pass
                        now = asyncio.get_running_loop().time()
                        crash_error = None
                        if now >= next_crashes:
                            try:
                                crash_files = await asyncio.wait_for(collect_crashes(device, output/'crashes'), timeout=20)
                            except Exception as error:
                                crash_error = type(error).__name__
                            next_crashes = now+30
                        write_status(output, state='snapshot-complete' if args.once else 'following',
                                     bundle=bundle, files=files, crashFiles=crash_files, crashServiceError=crash_error)
                        if args.once or (deadline and now >= deadline) or (output/'STOP').exists():
                            return
                        await asyncio.sleep(args.interval)
        except asyncio.CancelledError:
            raise
        except Exception as error:
            write_status(output, state='waiting-for-usb', error=type(error).__name__,
                         action='Connect USB, unlock iPhone and trust this computer; Husk must be installed')
            if args.once:
                raise RuntimeError('USB capture failed ('+type(error).__name__+'). Unlock/reconnect iPhone and check Husk is installed.') from None
            if (deadline and asyncio.get_running_loop().time() >= deadline) or (output/'STOP').exists():
                return
            await asyncio.sleep(3)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--once', action='store_true')
    ap.add_argument('--duration', type=float, default=0, help='Seconds to follow; 0 until Ctrl+C or STOP file')
    ap.add_argument('--interval', type=float, default=2)
    ap.add_argument('--output', type=Path, default=ROOT/'device-logs/latest')
    ap.add_argument('--bundle-id')
    ap.add_argument('--udid')
    args = ap.parse_args()
    if args.interval < 1 or args.duration < 0:
        ap.error('interval must be >= 1 and duration >= 0')
    if (args.output/'STOP').exists():
        ap.error('Remove the STOP file before starting another capture')
    try:
        with capture_lock(args.output):
            asyncio.run(capture(args))
    except ModuleNotFoundError:
        ap.exit(2, 'Run scripts/ios_logs.ps1 -Setup first to install the USB dependency.\n')
    except (RuntimeError, ValueError) as error:
        ap.exit(1, str(error)+'\n')
    except KeyboardInterrupt:
        pass
    finally:
        if not args.once:
            status_path=args.output/'status.json'
            if status_path.exists():
                status=json.loads(status_path.read_text(encoding='utf-8'))
                if status.get('collectorPID')==os.getpid():
                    status['state']='stopped'
                    write_status(args.output,**status)


if __name__ == '__main__':
    main()
