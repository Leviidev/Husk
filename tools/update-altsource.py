#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Regenerate the AltStore/SideStore source from GitHub releases.

    python3 tools/update-altsource.py

Writes altsource.json here (old address still works) and, when present,
../husk-work/site/web/public/repo.json (or $SITE_REPO_JSON) — that file is https://itslevii.dev/repo.json.
"""
import json
import os
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "altsource.json")
SITE = os.environ.get("SITE_REPO_JSON") or os.path.expanduser("~/husk-work/site/web/public/repo.json")
# Henge keeps its own source (made by HengeVM's tools/update-source.py); its app entry is copied from there.
HENGE_LOCAL = os.path.expanduser("~/Documents/Coding/HengeVM/repo.json")
HENGE_RAW = "https://raw.githubusercontent.com/Leviidev/Henge/main/repo.json"

HUSK_RAW = "https://raw.githubusercontent.com/Leviidev/Husk/main"
KEEP = 10


def gh(path):
    req = urllib.request.Request(
        "https://api.github.com" + path,
        headers={"User-Agent": "husk-altsource", "Accept": "application/vnd.github+json"},
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def ipa_versions(repo, keep=KEEP, include_prerelease=False):
    versions = []
    for rel in gh(f"/repos/Leviidev/{repo}/releases?per_page=30"):
        if rel.get("draft"):
            continue
        if rel.get("prerelease") and not include_prerelease:
            continue
        ipa = next((a for a in rel["assets"] if a["name"].lower().endswith(".ipa")), None)
        if not ipa:
            continue
        version = rel["tag_name"].lstrip("vV")
        notes = (rel.get("body") or "").strip() or f"{repo} {version}."
        versions.append({
            "version": version,
            "date": rel["published_at"],
            "localizedDescription": notes[:4000],
            "downloadURL": ipa["browser_download_url"],
            "size": ipa["size"],
        })
        if len(versions) >= keep:
            break
    return versions


def henge_app():
    if os.path.isfile(HENGE_LOCAL):
        with open(HENGE_LOCAL) as f:
            return json.load(f)["apps"][0]
    req = urllib.request.Request(HENGE_RAW, headers={"User-Agent": "husk-altsource"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)["apps"][0]


def with_min(versions, min_os):
    for v in versions:
        v["minOSVersion"] = min_os
    return versions


def shots():
    folder = os.path.join(ROOT, "Screenshots")
    names = sorted(f for f in os.listdir(folder) if f.lower().endswith((".png", ".jpeg", ".jpg")))
    return [f"{HUSK_RAW}/Screenshots/{s}" for s in names]


def main():
    husk_icon = f"{HUSK_RAW}/src/app/Husk/Assets.xcassets/AppIcon.appiconset/Icon-Default.png"
    husk_versions = with_min(ipa_versions("Husk"), "15.5")
    # Older Husk builds were 16.4 until 1.0.0
    for v in husk_versions:
        parts = [int(p) for p in v["version"].replace("-", ".").split(".") if p.isdigit()] + [0, 0, 0]
        if parts[:3] < [1, 0, 0]:
            v["minOSVersion"] = "16.4"
    # AltStore and SideStore compare these to the app's CFBundleVersion: add each release's here
    builds = {"3.4": "149", "3.3": "148"}
    for v in husk_versions:
        if v["version"] in builds:
            v["buildVersion"] = builds[v["version"]]

    apps = [
        {
            "name": "Husk",
            "bundleIdentifier": "com.husk.app",
            "developerName": "Leviidev",
            "subtitle": "Android apps and PS4 games on your iPhone.",
            "localizedDescription": (
                "Husk runs Android apps and games, and PlayStation 4 games, on iPhone. "
                "Its Translation Layer runs games made with Unity, Unreal, Godot, Flutter, cocos2d-x, SDL and more natively, "
                "a full Android system is there for everything else, and its PS4 layer plays fake packages and game folders (iOS 17.4+). "
                "Husk needs JIT: StikDebug, its built-in StikJIT, TrollStore or a jailbreak — or Play without JIT after you import a certificate."
            ),
            "iconURL": husk_icon,
            "tintColor": "#5865F2",
            "category": "utilities",
            "screenshots": shots(),
            "versions": husk_versions,
            "appPermissions": {
                "entitlements": [
                    "get-task-allow",
                    "dynamic-codesigning",
                    "com.apple.developer.kernel.increased-memory-limit",
                    "com.apple.developer.kernel.extended-virtual-addressing",
                ],
                "privacy": {
                    "NSLocalNetworkUsageDescription": "Husk uses the local network to pair with this iPhone for JIT, without a computer.",
                },
            },
        },
        {
            "name": "AetherPS4",
            "bundleIdentifier": "com.aether.ps4ios",
            "developerName": "Leviidev",
            "subtitle": "PS4 emulator for iPhone and iPad.",
            "localizedDescription": "AetherPS4 is a PlayStation 4 emulator for iOS and iPadOS: a standalone app plus the emulator core.",
            "iconURL": "https://raw.githubusercontent.com/Leviidev/AetherPS4/main/AetherPS4-iOS/Resources/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png",
            "tintColor": "#003087",
            "category": "games",
            "versions": with_min(ipa_versions("AetherPS4"), "17.4"),
        },
        {
            "name": "Utsuro",
            "bundleIdentifier": "com.utsuro.app",
            "developerName": "Leviidev",
            "subtitle": "Alpha — Xbox One games on iPhone.",
            "localizedDescription": (
                "Utsuro runs Xbox One games on iOS and iPadOS. This is an alpha release: Minecraft Xbox One Edition, "
                "Rocket League and Sonic Mania have been shown to start; expect bugs."
            ),
            "iconURL": "https://raw.githubusercontent.com/Leviidev/Utsuro/main/ios/Utsuro/Assets.xcassets/AppIcon.appiconset/icon.png",
            "tintColor": "#107C10",
            "category": "games",
            "beta": True,
            "versions": with_min(ipa_versions("Utsuro", include_prerelease=True), "17.4"),
        },
        {
            "name": "Podium",
            "bundleIdentifier": "com.leviidev.podium",
            "developerName": "Leviidev",
            "subtitle": "A classic iPod Touch on a modern iPhone.",
            "localizedDescription": (
                "Podium is a classic iPod Touch emulator. It currently targets iOS 6.1.6 on the iPod Touch (4th generation); "
                "other software versions and devices can be tried."
            ),
            "iconURL": "https://raw.githubusercontent.com/Leviidev/Podium/main/Podium/Resources/Assets.xcassets/AppIcon.appiconset/AppIcon.png",
            "tintColor": "#8E8E93",
            "category": "entertainment",
            "versions": with_min(ipa_versions("Podium"), "17.0"),
        },
        {
            "name": "Fathom",
            "bundleIdentifier": "app.fathom.emulator",
            "developerName": "Leviidev",
            "subtitle": "An x86-64 PC emulator for iPhone.",
            "localizedDescription": (
                "Fathom is an x86-64 PC emulator for iOS. It runs guest code through FEXCore. "
                "2048 and tic-tac-toe ship as sample programs."
            ),
            "iconURL": "https://raw.githubusercontent.com/Leviidev/Fathom/main/Fathom/Resources/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png",
            "tintColor": "#0E7490",
            "category": "utilities",
            "versions": with_min(ipa_versions("Fathom"), "18.0"),
        },
        henge_app(),
    ]

    source = {
        "name": "itslevii",
        "identifier": "dev.itslevii.source",
        "subtitle": "Husk, AetherPS4, Utsuro, Podium, Fathom and Henge.",
        "description": "Leviidev's apps. Add this source in SideStore or AltStore to install them and get updates.",
        "iconURL": husk_icon,
        "website": "https://itslevii.dev",
        "tintColor": "#5865F2",
        "featuredApps": ["com.husk.app", "com.henge.app"] + [a["bundleIdentifier"] for a in apps if a["bundleIdentifier"] not in ("com.husk.app", "com.henge.app")],
        "apps": apps,
        "news": [],
    }

    text = json.dumps(source, indent=2) + "\n"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write(text)
    print(f"wrote {os.path.relpath(OUT, ROOT)}: {len(apps)} apps")
    if os.path.isdir(os.path.dirname(SITE)):
        with open(SITE, "w") as f:
            f.write(text)
        print(f"wrote {SITE}")
    else:
        print(f"site repo.json not written ({SITE} missing)")


if __name__ == "__main__":
    main()
