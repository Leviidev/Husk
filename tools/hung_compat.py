#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Evidence-based APK analysis, log diagnostics and per-APK QA profiles.

Uses only Python's standard library. No APK is executed or changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import tempfile
import xml.etree.ElementTree as ET
import zipfile

STATUSES = ("PASS", "PARTIAL", "FAIL", "BLOCKED", "NOT TESTED")
STAGES = ("launch", "rendering", "mainMenu", "input", "resources", "gameplay")
ENGINE_LIBS = {
    "Unity / IL2CPP": ("libil2cpp.so",), "Unity": ("libunity.so",),
    "Unity / Mono": ("libmono.so", "libmonobdwgc-2.0.so"),
    "Unreal Engine": ("libUE4.so", "libUnreal.so"), "Godot": ("libgodot_android.so",),
    "SDL3": ("libSDL3.so",), "SDL2": ("libSDL2.so",),
    "Cocos2d-x": ("libcocos2dcpp.so", "libcocos2djs.so"),
}
ANDROID_NS = "{http://schemas.android.com/apk/res/android}"


def digest(path):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def _span(data, offset, size):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError("truncated or out-of-range binary structure")
    return data[offset:offset + size]


def _unpack(fmt, data, off):
    return struct.unpack(fmt, _span(data, off, struct.calcsize(fmt)))


def _cstring(data, off, end):
    if not 0 <= off < end <= len(data):
        raise ValueError("invalid string offset")
    stop = data.find(b"\0", off, end)
    if stop < 0:
        raise ValueError("unterminated string")
    return data[off:stop].decode("utf-8", errors="replace")


def _pool(chunk):
    count, _, flags, start, _ = _unpack("<5I", chunk, 8)
    header = _unpack("<H", chunk, 2)[0]
    if count > 250000 or header + count * 4 > len(chunk) or start < header + count * 4:
        raise ValueError("invalid manifest string pool")
    strings = []
    for i in range(count):
        at = start + _unpack("<I", chunk, header + i * 4)[0]
        def length(pos, utf8):
            if utf8:
                first = _span(chunk, pos, 1)[0]
                return (((first & 127) << 8) | _span(chunk, pos + 1, 1)[0], pos + 2) if first & 128 else (first, pos + 1)
            first = _unpack("<H", chunk, pos)[0]
            return (((first & 32767) << 16) | _unpack("<H", chunk, pos + 2)[0], pos + 4) if first & 32768 else (first, pos + 2)
        n, at = length(at, flags & 256)
        if flags & 256:
            n, at = length(at, True)
            strings.append(_span(chunk, at, n).decode("utf-8", errors="replace"))
        else:
            strings.append(_span(chunk, at, n * 2).decode("utf-16le", errors="replace"))
    return strings


def manifest(data):
    """Read textual XML or Android's typed binary XML, without apktool/Android SDK."""
    elements = []
    if data.lstrip().startswith(b"<"):
        root = ET.fromstring(data)
        elements = [(el.tag.split("}")[-1], {k.split("}")[-1]: v for k, v in el.attrib.items()}) for el in root.iter()]
    else:
        kind, header, size = _unpack("<HHI", data, 0)
        if kind != 3 or header < 8 or size != len(data):
            raise ValueError("invalid binary AndroidManifest.xml")
        strings = []
        at = header
        while at < size:
            kind, hsize, n = _unpack("<HHI", data, at)
            if n < hsize or hsize < 8 or at + n > size:
                raise ValueError("invalid manifest chunk")
            chunk = _span(data, at, n)
            if kind == 1:
                strings = _pool(chunk)
            elif kind == 0x102:
                if hsize < 16:
                    raise ValueError("short manifest element header")
                _, name, attr_start, attr_size, count = _unpack("<IIHHH", chunk, 16)
                if attr_size < 20 or count > 4096:
                    raise ValueError("invalid manifest attributes")
                attrs = {}
                for i in range(count):
                    a = _span(chunk, 16 + attr_start + i * attr_size, 20)
                    _, key, raw, _, _, typ, value = struct.unpack("<IIIHBBI", a)
                    if typ == 3:
                        v = strings[value]
                    elif typ in (0x10, 0x11, 0x12):
                        v = value
                    elif raw != 0xFFFFFFFF:
                        v = strings[raw]
                    else:
                        v = {"type": typ, "data": value}
                    attrs[strings[key]] = v
                elements.append((strings[name], attrs))
            at += n
    result = {"package": None, "minSdk": None, "targetSdk": None, "permissions": [], "features": [], "glesVersion": None}
    for name, attrs in elements:
        if name == "manifest":
            result.update({k: attrs[k] for k in ("package", "split", "versionCode", "versionName") if k in attrs})
        elif name == "uses-sdk":
            result["minSdk"] = attrs.get("minSdkVersion")
            result["targetSdk"] = attrs.get("targetSdkVersion")
        elif name in ("uses-permission", "uses-permission-sdk-23") and isinstance(attrs.get("name"), str):
            result["permissions"].append(attrs["name"])
        elif name == "uses-feature":
            result["features"].append(attrs)
            if "glEsVersion" in attrs:
                result["glesVersion"] = attrs["glEsVersion"]
    result["permissions"] = sorted(set(result["permissions"]))
    return result


def dex_api_refs(data):
    if len(data) < 112 or not data.startswith(b"dex\n"):
        raise ValueError("unsupported DEX header (compact DEX is not decoded)")
    ns, so, nt, to = _unpack("<4I", data, 56)
    nm, mo = _unpack("<2I", data, 88)
    if max(ns, nt, nm) > 250000:
        raise ValueError("DEX index count exceeds analysis limit")
    def string(index):
        if index >= ns:
            raise ValueError("invalid DEX string index")
        at = _unpack("<I", data, so + index * 4)[0]
        for _ in range(5):
            byte = _span(data, at, 1)[0]; at += 1
            if not byte & 128:
                return _cstring(data, at, len(data))
        raise ValueError("invalid DEX string length")
    types = [string(_unpack("<I", data, to + i * 4)[0]) for i in range(nt)]
    refs = set()
    for i in range(nm):
        cls, _, name = _unpack("<HHI", data, mo + i * 8)
        if cls >= nt:
            raise ValueError("invalid DEX class index")
        if types[cls].startswith(("Landroid/", "Ljava/", "Ljavax/", "Lorg/libsdl/")):
            refs.add(types[cls] + "->" + string(name))
    return sorted(refs)


def elf_report(data):
    if data[:4] != b"\x7fELF" or len(data) < 52 or data[5] != 1 or data[4] not in (1, 2):
        raise ValueError("expected little-endian ELF32 or ELF64")
    wide = data[4] == 2
    machine = _unpack("<H", data, 18)[0]
    if _unpack("<H", data, 16)[0] != 3:
        raise ValueError("ELF library must have ET_DYN type")
    phoff = _unpack("<Q" if wide else "<I", data, 32 if wide else 28)[0]
    phsize, phcount = _unpack("<HH", data, 54 if wide else 42)
    if phcount > 4096 or phsize < (56 if wide else 32):
        raise ValueError("invalid ELF program headers")
    loads, dynamic = [], None
    for i in range(phcount):
        p = _span(data, phoff + i * phsize, phsize)
        if wide:
            typ, _, off, va, _, filesz, memsz, _ = _unpack("<II6Q", p, 0)
        else:
            typ, off, va, _, filesz, memsz, _, _ = _unpack("<8I", p, 0)
        _span(data, off, filesz)
        if typ == 1:
            loads.append((va, off, filesz))
        elif typ == 2:
            dynamic = (off, filesz)
    def address(va, size):
        for base, off, n in loads:
            if va >= base and va - base + size <= n:
                return off + va - base
        raise ValueError("ELF virtual address outside file-backed segments")
    result = {"machine": machine, "bits": 64 if wide else 32, "needed": [], "imports": [], "exports": [], "analysisComplete": False}
    if dynamic is None:
        return result
    tags, needed = {}, []
    step = 16 if wide else 8
    for i in range(0, min(dynamic[1], 65536 * step), step):
        tag, value = _unpack("<qQ" if wide else "<iI", data, dynamic[0] + i)
        if tag == 0:
            break
        tags[tag] = value
        if tag == 1:
            needed.append(value)
    if not tags.get(5) or not tags.get(10):
        return result
    stroff = address(tags[5], tags[10])
    def string(index):
        if index >= tags[10]:
            raise ValueError("ELF string index outside DT_STRTAB")
        return _cstring(data, stroff + index, stroff + tags[10])
    result["needed"] = [string(i) for i in needed]
    count = 0
    if 4 in tags:
        count = _unpack("<I", data, address(tags[4], 8) + 4)[0]
    elif 0x6FFFFEF5 in tags:
        base = tags[0x6FFFFEF5]
        buckets, first, bloom, _ = _unpack("<4I", data, address(base, 16))
        if max(buckets, first, bloom) > 250000:
            raise ValueError("ELF GNU hash exceeds analysis limit")
        bva = base + 16 + bloom * (8 if wide else 4)
        boff = address(bva, buckets * 4)
        last = max((_unpack("<I", data, boff + i * 4)[0] for i in range(buckets)), default=0)
        count = max(first, last)
        if last:
            while count < 250000:
                value = _unpack("<I", data, address(bva + buckets * 4 + (count - first) * 4, 4))[0]
                count += 1
                if value & 1:
                    break
    if not count or count > 250000 or 6 not in tags:
        return result
    size = tags.get(11, 24 if wide else 16)
    if size != (24 if wide else 16):
        raise ValueError("invalid ELF dynamic symbol size")
    symoff = address(tags[6], count * size)
    for i in range(1, count):
        s = _span(data, symoff + i * size, size)
        index = _unpack("<I", s, 0)[0]
        if not index:
            continue
        name = string(index)
        info = s[4 if wide else 12]
        section = _unpack("<H", s, 6 if wide else 14)[0]
        if section == 0:
            result["imports"].append({"name": name, "weak": info >> 4 == 2})
        elif info >> 4 in (1, 2):
            result["exports"].append(name)
    result["analysisComplete"] = True
    return result


def analyze(paths):
    result = {"schemaVersion": 1, "assessment": "NOT TESTED", "apks": [], "libraries": [], "dex": [], "warnings": []}
    abis, libraries, apis, manifests, assets = set(), set(), set(), [], []
    for path in map(Path, paths):
        sha = digest(path)
        result["apks"].append({"file": path.name, "sha256": sha})
        with zipfile.ZipFile(path) as z:
            names = z.namelist()
            if len(names) != len(set(names)):
                raise ValueError("duplicate APK entry names; analysis is ambiguous")
            if "AndroidManifest.xml" not in names:
                raise ValueError("APK lacks AndroidManifest.xml")
            if z.getinfo("AndroidManifest.xml").file_size > 16 << 20:
                raise ValueError("manifest exceeds analysis limit")
            meta = manifest(z.read("AndroidManifest.xml"))
            if not isinstance(meta.get("package"), str) or not meta["package"]:
                raise ValueError("manifest package must be a nonempty string")
            manifests.append(meta)
            for item in z.infolist():
                parts = item.filename.split("/")
                if len(parts) == 3 and parts[0] == "lib" and parts[2].endswith(".so"):
                    abis.add(parts[1]); libraries.add(parts[2])
                    row = {"apk": path.name, "entry": item.filename, "abi": parts[1], "bytes": item.file_size}
                    try:
                        if item.file_size > 192 << 20:
                            raise ValueError("library exceeds 192 MiB static analysis budget")
                        row.update(elf_report(z.read(item)))
                        expected = {"arm64-v8a": (183, 64), "armeabi-v7a": (40, 32), "armeabi": (40, 32), "x86": (3, 32), "x86_64": (62, 64)}.get(parts[1])
                        row["abiMatchesELF"] = expected == (row["machine"], row["bits"]) if expected else None
                        if row["abiMatchesELF"] is False:
                            result["warnings"].append(f"{item.filename}: declared ABI does not match the ELF machine/class")
                    except (ValueError, struct.error, IndexError) as e:
                        row.update({"analysisComplete": False, "error": str(e)})
                    result["libraries"].append(row)
                elif re.fullmatch(r"classes\d*\.dex", item.filename):
                    row = {"apk": path.name, "entry": item.filename, "bytes": item.file_size}
                    try:
                        if item.file_size > 128 << 20:
                            raise ValueError("DEX exceeds analysis budget")
                        refs = dex_api_refs(z.read(item))
                        apis.update(refs)
                        row["methodReferenceCount"] = len(refs)
                    except (ValueError, struct.error, IndexError) as e:
                        row["error"] = str(e)
                    result["dex"].append(row)
                if item.filename.endswith(".obb") or item.filename.startswith(("assetpacks/", "assets/assetpacks/")):
                    assets.append({"apk": path.name, "entry": item.filename})
    packages = {m.get("package") for m in manifests}
    if len(packages) != 1 or None in packages:
        raise ValueError("base and split APKs must declare the same package")
    result["package"] = packages.pop()
    result["manifests"] = manifests
    result["abis"] = sorted(abis)
    result["engines"] = [{"name": engine, "evidence": sorted(libraries.intersection(names))}
                          for engine, names in ENGINE_LIBS.items() if libraries.intersection(names)]
    if not result["engines"]:
        result["engines"] = [{"name": "Java / Kotlin candidate" if result["dex"] else "Native / custom / unknown",
                              "evidence": [d["entry"] for d in result["dex"]]}]
    result["androidAPIReferences"] = sorted(apis)
    result["apiReferenceMeaning"] = "DEX method-table references; not proof of invocation or API compatibility"
    result["permissions"] = sorted({p for m in manifests for p in m["permissions"]})
    result["internetDeclared"] = "android.permission.INTERNET" in result["permissions"]
    result["additionalDataEvidence"] = assets
    result["externalDataRequirement"] = "unknown; external OBB/downloads/Play asset delivery cannot be excluded by ZIP contents"
    imports = {i["name"] for lib in result["libraries"] for i in lib.get("imports", [])}
    result["graphics"] = {"manifestGLES": [m["glesVersion"] for m in manifests if m["glesVersion"] is not None],
                          "vulkanImportEvidence": sorted(i for i in imports if i.startswith("vk")),
                          "glesImportEvidence": sorted(i for i in imports if i.startswith(("gl", "egl"))),
                          "deviceFeatureSupport": "NOT TESTED; shaders, texture formats and swapchain require runtime verification"}
    arm = bool(abis.intersection(("arm64-v8a", "armeabi-v7a", "armeabi")))
    result["execution"] = {"translation": "candidate" if arm or not abis else "unsupported ABI",
                           "arm32": "experimental" if "arm64-v8a" not in abis and arm else None,
                           "emulation": "manual only; needs installed ARM64 Android guest, JIT and separate app data",
                           "automaticFallback": False}
    hashes = sorted(a["sha256"] for a in result["apks"])
    result["apkFingerprint"] = hashes[0] if len(hashes) == 1 else hashlib.sha256(("split:" + "\n".join(hashes)).encode()).hexdigest()
    if not arm and abis:
        result["warnings"].append("No ARM native code; the current ARM64 Android guest does not by itself solve x86/x86_64 compatibility")
    return result


def diagnose(text, startup_window_expired=False):
    lines = text.splitlines()
    loaded, called, issues = {}, {}, []
    for line in lines:
        match = re.search(r"ld: (\S+): unresolved import (\S+)", line)
        if match:
            loaded.setdefault(match[2], []).append(match[1])
        match = re.search(r"CALLED an unresolved import: (\S+)", line)
        if match:
            called.setdefault(match[1], line)
    for name in sorted(loaded.keys() | called.keys()):
        active = name in called
        issues.append({"code": "NATIVE_SYMBOL_CALLED" if active else "NATIVE_SYMBOL_UNRESOLVED",
                       "symbol": name, "severity": "error" if active else "info", "status": "UNFIXED" if active else "UNVERIFIED",
                       "suspectedCause": "Missing native function was invoked; its contribution to a stall is not established" if active else "Unresolved at relocation; invocation not observed",
                       "evidence": called.get(name, ", ".join(loaded.get(name, []))),
                       "remediation": "Implement an ABI-correct wrapper or supply the matching Android dependency; retest on device"})
    rules = (
        ("JIT_UNAVAILABLE", r"no StikDebug JIT region|selftest:\s*FAIL|JIT is not enabled", "Enable JIT with the existing debugger flow"),
        ("UNSUPPORTED_ABI", r"Native code for x86 only|unsupported (?:CPU|ABI)|wrong kind of CPU", "Use an ARM build; do not assume an ARM emulator translates x86"),
        ("JNI_EXCEPTION", r"jni: (?:throwing|pending exception)|main thread ended with|JNI_OnLoad.*pending exception", "Inspect the exception and its first failing API/native method"),
        ("GRAPHICS_INIT_FAILED", r"(?:EGL|ANGLE|Vulkan|Metal|eglInitialize).*(?:failed|FAILED|unavailable)|Couldn't open.*ANGLE", "Verify the embedded graphics backend and the required device features"),
        ("MISSING_GAME_RESOURCES", r"expansion file \(OBB\) is missing|missing game (?:data|resources)", "Import legitimate OBB/splits/assets for this APK version"),
        ("FILE_ACCESS_FAILED", r"file: (?:open|stat|access).*-> -1|Permission denied", "Check the resolved sandbox path and installed game data"),
        ("NETWORK_TIMEOUT", r"(?i:network.*timed out|connect.*timed out|NSURLErrorDomain.*-1001|SocketTimeoutException)", "Check the connection and endpoint logs; do not alter server data"),
        ("PERMISSION_FAILED", r"SecurityException.*(?:permission|Permission)|EACCES|EPERM", "Inspect the requested permission and whether iOS can supply the underlying service"),
        ("NATIVE_CRASH", r"=== CRASH:|\*\*\* FATAL SIGNAL", "Symbolicate the faulting library and inspect the crash report"),
        ("OUT_OF_MEMORY", r"OutOfMemoryError|jetsam.*(?:killed|terminated)|allocation.*failed.*ENOMEM", "Measure memory pressure and reduce resolution or memory demand"),
        ("EMULATOR_BOOT_FAILED", r"(?:QEMU|qemu|guest boot).*(?:failed|FAILED)|Kernel panic", "Inspect the QEMU boot log and installed guest image"),
    )
    for code, pattern, remedy in rules:
        evidence = next((line for line in reversed(lines) if re.search(pattern, line)), None)
        if evidence:
            issues.append({"code": code, "severity": "error", "status": "UNVERIFIED", "suspectedCause": code.replace("_", " ").lower(),
                           "evidence": evidence, "remediation": remedy})
    if startup_window_expired:
        issues.append({"code": "STARTUP_WINDOW_EXPIRED", "severity": "warning", "status": "UNVERIFIED",
                       "suspectedCause": "User-observed startup did not complete in the observation window; root cause unknown",
                       "evidence": "Explicit --startup-window-expired observation", "remediation": "Compare lifecycle, encoding, resources and network evidence; rendering alone is insufficient"})
    return {"schemaVersion": 1, "issues": issues, "lastStartupMarker": next((l for l in reversed(lines) if "startup:" in l), None),
            "jitSelftestPassed": any(re.search(r"selftest:\s*PASS", l) for l in lines),
            "iconvEvidence": [l for l in lines if "iconv:" in l][-32:],
            "gameplay": "NOT TESTED", "rootCause": "unverified; multiple issues may coexist"}


def result_status(results):
    values = [results.get(stage, "NOT TESTED") for stage in STAGES]
    if any(v not in STATUSES for v in values):
        raise ValueError("unknown QA status")
    if all(v == "PASS" for v in values):
        return "PASS"
    if "FAIL" in values:
        return "FAIL"
    if "BLOCKED" in values:
        return "BLOCKED"
    if any(v in ("PASS", "PARTIAL") for v in values):
        return "PARTIAL"
    return "NOT TESTED"


def write_json(path, value, exclusive=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(value, ensure_ascii=False, indent=2) + "\n"
    if exclusive:
        with path.open("x", encoding="utf-8") as f:
            f.write(text)
        return
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, delete=False) as f:
        temp = Path(f.name)
        f.write(text)
    try:
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)
    scan = sub.add_parser("analyze"); scan.add_argument("apks", nargs="+", type=Path); scan.add_argument("--out", type=Path)
    log = sub.add_parser("diagnose"); log.add_argument("log", type=Path); log.add_argument("--startup-window-expired", action="store_true"); log.add_argument("--out", type=Path)
    profile = sub.add_parser("profile"); profile.add_argument("report", type=Path); profile.add_argument("--out", type=Path, required=True)
    record = sub.add_parser("record"); record.add_argument("profile", type=Path); record.add_argument("--device", required=True); record.add_argument("--evidence", required=True)
    for stage in STAGES:
        record.add_argument("--" + stage, choices=STATUSES)
    matrix = sub.add_parser("matrix"); matrix.add_argument("profiles", nargs="+", type=Path)
    args = ap.parse_args()
    if args.cmd == "analyze":
        value = analyze(args.apks)
    elif args.cmd == "diagnose":
        value = diagnose(args.log.read_text(encoding="utf-8", errors="replace"), args.startup_window_expired)
    elif args.cmd == "profile":
        report = json.loads(args.report.read_text(encoding="utf-8"))
        value = {"schemaVersion": 1, "package": report["package"], "apkFingerprint": report["apkFingerprint"],
                 "engines": report["engines"], "configuration": {"mode": "manual", "orientation": "auto", "resolution": "high", "controller": "auto"},
                 "results": {stage: "NOT TESTED" for stage in STAGES}, "result": "NOT TESTED", "evidence": [],
                 "note": "QA profile; runtime settings live in the iOS app's per-app settings.json"}
        write_json(args.out, value, exclusive=True)
        return
    elif args.cmd == "record":
        value = json.loads(args.profile.read_text(encoding="utf-8"))
        if not re.fullmatch(r"[0-9a-f]{64}", value.get("apkFingerprint") or ""):
            ap.error("recording requires a profile created from the exact APK analysis")
        if not args.evidence.strip() or not args.device.strip():
            ap.error("device and evidence must be nonempty")
        for stage in STAGES:
            if getattr(args, stage):
                value["results"][stage] = getattr(args, stage)
        value["result"] = result_status(value["results"])
        value["evidence"].append({"device": args.device, "observation": args.evidence,
                                  "results": dict(value["results"])})
        write_json(args.profile, value)
        return
    else:
        print("| Game | Engine | Launch | Rendering | Input | Gameplay | Result |")
        print("|---|---|---|---|---|---|---|")
        for path in args.profiles:
            p = json.loads(path.read_text(encoding="utf-8"))
            cells = [p["package"], ", ".join(e["name"] for e in p["engines"])]
            cells += [p["results"].get(stage, "NOT TESTED") for stage in ("launch", "rendering", "input", "gameplay")]
            cells += [result_status(p["results"])]
            print("| " + " | ".join(str(c).replace("|", "\\|").replace("\n", " ") for c in cells) + " |")
        return
    if args.out:
        write_json(args.out, value)
    else:
        print(json.dumps(value, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, IndexError, RuntimeError, NotImplementedError, struct.error, zipfile.BadZipFile, ET.ParseError) as error:
        raise SystemExit(f"analysis failed: {error}")
