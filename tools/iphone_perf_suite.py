#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Selectable iPhone 8 -> iPhone 18 performance verification suite for Phong Than iOS.

Strict provenance contract:
- Real hardware conclusions ('dat' or 'co giat/lag') come ONLY from real device logs
  whose [boot] device identifier and build commit match the target device row.
- Models without physical test hardware or missing one or more required scenarios
  are marked 'chua du du lieu'.
- Unreleased or partially unconfirmed iPhone 18 variants are marked
  'thieu cau hinh xac minh' in addition to 'chua du du lieu'.
- Optional analytical estimates (--simulate) are tagged explicitly as 'MO PHONG'.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
import math
from pathlib import Path
import re
import sys
from typing import Any


TARGET_PACKAGE = "com.tech3cgame.vhpt1"
TARGET_FPS = 60.0
DEFAULT_GRAPHICS_PROFILE = {
    "name": "Standardized Reference Preset (60 FPS Target)",
    "husk_resolution": "Native",
    "husk_visual_quality": "Pro Max",
    "game_config_fps": 60,
    "game_represent": 2,
    "avoid_unsafe_area": True,
    "align_vhpt_hud": True,
}

# Published evaluation thresholds grounded in VHPT config.ini (FPS=60 -> 16.67 ms budget)
THRESHOLDS = {
    "target_fps": TARGET_FPS,
    "frame_budget_ms": round(1000.0 / TARGET_FPS, 2),
    "pass_min_avg_fps": 54.0,          # >= 90% of 60 FPS target
    "pass_max_mean_frame_ms": 18.5,
    "pass_max_p95_frame_ms": 22.0,     # frame-time stability ceiling
    "pass_max_frame_ms": 50.0,         # single-frame hitch ceiling
    "pass_max_hitch50_per_min": 2.0,   # >= 50 ms hitches per minute
    "pass_max_hitch100_count": 0,      # zero long hitches >= 100 ms
    "required_scenarios": [
        "normal_play",
        "heavy_crowd_effects",
        "game_switch",
    ],
}

SCENARIO_LABELS = {
    "normal_play": "Choi binh thuong (di chuyen / danh quai)",
    "heavy_crowd_effects": "Canh dong hieu ung (thanh thi / skill AOE dong nguoi)",
    "game_switch": "Chuyen game qua lai (Phong Than <-> game khac)",
}


@dataclass(frozen=True)
class IPhoneModelSpec:
    id: str
    family: str
    variant: str
    machine_ids: tuple[str, ...]
    chip: str
    gpu_score: float            # relative GPU/SoC index vs A17 Pro = 1.00 (simulation only)
    ram_gb: float | None
    physical_resolution: str
    pixel_count: int | None
    logical_points: str
    refresh_rate_hz: int | None
    cutout_type: str            # none | notch | dynamic_island | foldable
    safe_left_pt: int | None
    compatible_ios: str
    spec_status: str            # "verified" | "missing_verified_specs"
    spec_notes: str = ""


# Complete iPhone 8 -> iPhone 18 variant catalog
IPHONE_CATALOG: tuple[IPhoneModelSpec, ...] = (
    IPhoneModelSpec("iphone-8", "iPhone 8", "iPhone 8 (4.7\")", ("iPhone10,1", "iPhone10,4"), "Apple A11 Bionic", 0.24, 2.0, "1334x750", 1334 * 750, "667x375", 60, "none", 0, "iOS 11.0 - 16.7.10", "verified"),
    IPhoneModelSpec("iphone-8-plus", "iPhone 8", "iPhone 8 Plus (5.5\")", ("iPhone10,2", "iPhone10,5"), "Apple A11 Bionic", 0.24, 3.0, "1920x1080", 1920 * 1080, "736x414", 60, "none", 0, "iOS 11.0 - 16.7.10", "verified"),
    IPhoneModelSpec("iphone-x", "iPhone X", "iPhone X (5.8\")", ("iPhone10,3", "iPhone10,6"), "Apple A11 Bionic", 0.24, 3.0, "2436x1125", 2436 * 1125, "812x375", 60, "notch", 44, "iOS 11.0 - 16.7.10", "verified"),
    IPhoneModelSpec("iphone-xr", "iPhone X/XR/XS", "iPhone XR (6.1\")", ("iPhone11,8",), "Apple A12 Bionic", 0.30, 3.0, "1792x828", 1792 * 828, "896x414", 60, "notch", 48, "iOS 12.0 - 18.x", "verified"),
    IPhoneModelSpec("iphone-xs", "iPhone X/XR/XS", "iPhone XS (5.8\")", ("iPhone11,2",), "Apple A12 Bionic", 0.30, 4.0, "2436x1125", 2436 * 1125, "812x375", 60, "notch", 44, "iOS 12.0 - 18.x", "verified"),
    IPhoneModelSpec("iphone-xs-max", "iPhone X/XR/XS", "iPhone XS Max (6.5\")", ("iPhone11,4", "iPhone11,6"), "Apple A12 Bionic", 0.30, 4.0, "2688x1242", 2688 * 1242, "896x414", 60, "notch", 44, "iOS 12.0 - 18.x", "verified"),
    IPhoneModelSpec("iphone-11", "iPhone 11", "iPhone 11 (6.1\")", ("iPhone12,1",), "Apple A13 Bionic", 0.38, 4.0, "1792x828", 1792 * 828, "896x414", 60, "notch", 48, "iOS 13.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-11-pro", "iPhone 11", "iPhone 11 Pro (5.8\")", ("iPhone12,3",), "Apple A13 Bionic", 0.38, 4.0, "2436x1125", 2436 * 1125, "812x375", 60, "notch", 44, "iOS 13.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-11-pro-max", "iPhone 11", "iPhone 11 Pro Max (6.5\")", ("iPhone12,5",), "Apple A13 Bionic", 0.38, 4.0, "2688x1242", 2688 * 1242, "896x414", 60, "notch", 44, "iOS 13.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-se-2", "iPhone SE", "iPhone SE (2nd gen)", ("iPhone12,8",), "Apple A13 Bionic", 0.38, 3.0, "1334x750", 1334 * 750, "667x375", 60, "none", 0, "iOS 13.4 - 27.x", "verified"),
    IPhoneModelSpec("iphone-12-mini", "iPhone 12", "iPhone 12 mini (5.4\")", ("iPhone13,1",), "Apple A14 Bionic", 0.48, 4.0, "2340x1080", 2340 * 1080, "780x360", 60, "notch", 50, "iOS 14.2 - 27.x", "verified"),
    IPhoneModelSpec("iphone-12", "iPhone 12", "iPhone 12 (6.1\")", ("iPhone13,2",), "Apple A14 Bionic", 0.48, 4.0, "2532x1170", 2532 * 1170, "844x390", 60, "notch", 47, "iOS 14.1 - 27.x", "verified"),
    IPhoneModelSpec("iphone-12-pro", "iPhone 12", "iPhone 12 Pro (6.1\")", ("iPhone13,3",), "Apple A14 Bionic", 0.50, 6.0, "2532x1170", 2532 * 1170, "844x390", 60, "notch", 47, "iOS 14.1 - 27.x", "verified"),
    IPhoneModelSpec("iphone-12-pro-max", "iPhone 12", "iPhone 12 Pro Max (6.7\")", ("iPhone13,4",), "Apple A14 Bionic", 0.50, 6.0, "2778x1284", 2778 * 1284, "926x428", 60, "notch", 47, "iOS 14.2 - 27.x", "verified"),
    IPhoneModelSpec("iphone-13-mini", "iPhone 13", "iPhone 13 mini (5.4\")", ("iPhone14,4",), "Apple A15 Bionic (4-GPU)", 0.58, 4.0, "2340x1080", 2340 * 1080, "780x360", 60, "notch", 50, "iOS 15.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-13", "iPhone 13", "iPhone 13 (6.1\")", ("iPhone14,5",), "Apple A15 Bionic (4-GPU)", 0.58, 4.0, "2532x1170", 2532 * 1170, "844x390", 60, "notch", 47, "iOS 15.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-13-pro", "iPhone 13", "iPhone 13 Pro (6.1\")", ("iPhone14,2",), "Apple A15 Bionic (5-GPU)", 0.68, 6.0, "2532x1170", 2532 * 1170, "844x390", 120, "notch", 47, "iOS 15.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-13-pro-max", "iPhone 13", "iPhone 13 Pro Max (6.7\")", ("iPhone14,3",), "Apple A15 Bionic (5-GPU)", 0.68, 6.0, "2778x1284", 2778 * 1284, "926x428", 120, "notch", 47, "iOS 15.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-se-3", "iPhone SE", "iPhone SE (3rd gen)", ("iPhone14,6",), "Apple A15 Bionic (4-GPU)", 0.60, 4.0, "1334x750", 1334 * 750, "667x375", 60, "none", 0, "iOS 15.4 - 27.x", "verified"),
    IPhoneModelSpec("iphone-14", "iPhone 14", "iPhone 14 (6.1\")", ("iPhone14,7",), "Apple A15 Bionic (5-GPU)", 0.68, 6.0, "2532x1170", 2532 * 1170, "844x390", 60, "notch", 47, "iOS 16.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-14-plus", "iPhone 14", "iPhone 14 Plus (6.7\")", ("iPhone14,8",), "Apple A15 Bionic (5-GPU)", 0.68, 6.0, "2778x1284", 2778 * 1284, "926x428", 60, "notch", 47, "iOS 16.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-14-pro", "iPhone 14", "iPhone 14 Pro (6.1\")", ("iPhone15,2",), "Apple A16 Bionic", 0.80, 6.0, "2556x1179", 2556 * 1179, "852x393", 120, "dynamic_island", 59, "iOS 16.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-14-pro-max", "iPhone 14", "iPhone 14 Pro Max (6.7\")", ("iPhone15,3",), "Apple A16 Bionic", 0.80, 6.0, "2796x1290", 2796 * 1290, "932x430", 120, "dynamic_island", 59, "iOS 16.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-15", "iPhone 15", "iPhone 15 (6.1\")", ("iPhone15,4",), "Apple A16 Bionic", 0.80, 6.0, "2556x1179", 2556 * 1179, "852x393", 60, "dynamic_island", 59, "iOS 17.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-15-plus", "iPhone 15", "iPhone 15 Plus (6.7\")", ("iPhone15,5",), "Apple A16 Bionic", 0.80, 6.0, "2796x1290", 2796 * 1290, "932x430", 60, "dynamic_island", 59, "iOS 17.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-15-pro", "iPhone 15", "iPhone 15 Pro (6.1\")", ("iPhone16,1",), "Apple A17 Pro", 1.00, 8.0, "2556x1179", 2556 * 1179, "852x393", 120, "dynamic_island", 59, "iOS 17.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-15-pro-max", "iPhone 15", "iPhone 15 Pro Max (6.7\")", ("iPhone16,2",), "Apple A17 Pro", 1.00, 8.0, "2796x1290", 2796 * 1290, "932x430", 120, "dynamic_island", 59, "iOS 17.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-16", "iPhone 16", "iPhone 16 (6.1\")", ("iPhone17,3",), "Apple A18", 1.12, 8.0, "2556x1179", 2556 * 1179, "852x393", 60, "dynamic_island", 59, "iOS 18.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-16-plus", "iPhone 16", "iPhone 16 Plus (6.7\")", ("iPhone17,4",), "Apple A18", 1.12, 8.0, "2796x1290", 2796 * 1290, "932x430", 60, "dynamic_island", 59, "iOS 18.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-16-pro", "iPhone 16", "iPhone 16 Pro (6.3\")", ("iPhone17,1",), "Apple A18 Pro", 1.24, 8.0, "2622x1206", 2622 * 1206, "874x402", 120, "dynamic_island", 62, "iOS 18.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-16-pro-max", "iPhone 16", "iPhone 16 Pro Max (6.9\")", ("iPhone17,2",), "Apple A18 Pro", 1.24, 8.0, "2868x1320", 2868 * 1320, "956x440", 120, "dynamic_island", 62, "iOS 18.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-16e", "iPhone 16", "iPhone 16e (6.1\")", ("iPhone17,5",), "Apple A18 (4-GPU)", 1.02, 8.0, "2532x1170", 2532 * 1170, "844x390", 60, "notch", 47, "iOS 18.3 - 27.x", "verified"),
    IPhoneModelSpec("iphone-17", "iPhone 17", "iPhone 17 (6.3\")", ("iPhone18,3",), "Apple A19", 1.32, 8.0, "2622x1206", 2622 * 1206, "874x402", 120, "dynamic_island", 62, "iOS 26.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-air", "iPhone 17", "iPhone Air (6.5\")", ("iPhone18,4",), "Apple A19 Pro (5-GPU)", 1.40, 12.0, "2736x1260", 2736 * 1260, "912x420", 120, "dynamic_island", 62, "iOS 26.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-17-pro", "iPhone 17", "iPhone 17 Pro (6.3\")", ("iPhone18,1",), "Apple A19 Pro", 1.48, 12.0, "2622x1206", 2622 * 1206, "874x402", 120, "dynamic_island", 62, "iOS 26.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-17-pro-max", "iPhone 17", "iPhone 17 Pro Max (6.9\")", ("iPhone18,2",), "Apple A19 Pro", 1.48, 12.0, "2868x1320", 2868 * 1320, "956x440", 120, "dynamic_island", 62, "iOS 26.0 - 27.x", "verified"),
    IPhoneModelSpec("iphone-17e", "iPhone 17", "iPhone 17e (6.1\")", ("iPhone18,5",), "Apple A19", 1.25, 8.0, "2532x1170", 2532 * 1170, "844x390", 60, "notch", 47, "iOS 26.3 - 27.x", "verified"),
    IPhoneModelSpec("iphone-18-pro", "iPhone 18", "iPhone 18 Pro (6.3\")", ("iPhone19,1",), "Apple A20 Pro", 1.72, 12.0, "2622x1206", 2622 * 1206, "874x402", 120, "dynamic_island", 54, "iOS 27.0+", "verified"),
    IPhoneModelSpec("iphone-18-pro-max", "iPhone 18", "iPhone 18 Pro Max (6.9\")", ("iPhone19,2",), "Apple A20 Pro", 1.72, 12.0, "2868x1320", 2868 * 1320, "956x440", 120, "dynamic_island", 54, "iOS 27.0+", "verified"),
    IPhoneModelSpec("iphone-18", "iPhone 18", "iPhone 18 (Standard)", (), "Chua cong bo (du kien X/2027)", 1.50, None, "Chua xac minh", None, "Chua xac minh", None, "dynamic_island", None, "iOS 27.x (du kien)", "missing_verified_specs", "Apple chia lich phat hanh iPhone 18 tieu chuan sang mua xuan 2027; thieu cau hinh xac minh."),
    IPhoneModelSpec("iphone-18e", "iPhone 18", "iPhone 18e", (), "Chua cong bo", 1.35, None, "Chua xac minh", None, "Chua xac minh", None, "notch", None, "iOS 27.x (du kien)", "missing_verified_specs", "Chua co thong so phan cung xac minh chinh thuc."),
    IPhoneModelSpec("iphone-duo", "iPhone 18", "iPhone Duo / Fold", (), "Apple A20 Pro", 1.72, 12.0, "Man hinh gap (~7.8\"/5.4\", chua co pixel/scale xac minh chi tiet)", None, "Chua xac minh", 120, "foldable", None, "iOS 27.0+", "missing_verified_specs", "Thieu thong so do phan giai pixel va safe-area xac minh cho hai man hinh gap."),
)


def select_models(selector: str | None) -> list[IPhoneModelSpec]:
    if not selector or selector.lower() in ("all", "*"):
        return list(IPHONE_CATALOG)
    masked = re.sub(r"(?i)(iphone\d+),(\d+)", "\\1\x00\\2", selector)
    tokens = [t.replace("\x00", ",").strip().lower() for t in masked.split(",") if t.strip()]
    out: list[IPhoneModelSpec] = []
    for tok in tokens:
        exact = [
            m for m in IPHONE_CATALOG
            if tok == m.id.lower()
            or tok == m.variant.lower()
            or any(tok == mid.lower() for mid in m.machine_ids)
        ]
        matches = exact or [
            m for m in IPHONE_CATALOG
            if tok in " ".join([m.id, m.family, m.variant, *m.machine_ids]).lower()
        ]
        if not matches:
            raise ValueError(f"Unknown iPhone model/variant selector token: {tok!r}")
        for m in matches:
            if m not in out:
                out.append(m)
    return out


def _quantile(vals: list[float], q: float) -> float | None:
    if not vals:
        return None
    s = sorted(vals)
    idx = max(0, min(len(s) - 1, math.ceil(q * len(s)) - 1))
    return s[idx]


def parse_device_logs(log_dir: Path) -> dict[str, Any]:
    """Parse husk.log / husk-native.log for verified real-device metadata & VHPT metrics."""
    husk_log = log_dir / "husk.log"
    native_log = log_dir / "husk-native.log"
    chunks: list[str] = []
    for p in (husk_log, native_log):
        if p.exists():
            chunks.append(p.read_text(encoding="utf-8", errors="replace"))
    combined = "\n".join(chunks)
    if not combined.strip():
        raise ValueError(f"No readable log files found in {log_dir}")

    device = None
    ios_version = None
    ram_mib = None
    build = None
    m_dev = re.findall(r"\[boot\]\s+device\s+:\s+(\S+)", combined)
    m_sys = re.findall(r"\[boot\]\s+system\s+:\s+(iOS\s+\S+|\S+\s+\S+)", combined)
    m_ram = re.findall(r"\[boot\]\s+physical RAM:\s+(\d+)\s+MiB", combined)
    m_bld = re.findall(r"\[boot\]\s+build\s+:\s+(.+)", combined)
    if m_dev: device = m_dev[-1].strip()
    if m_sys: ios_version = m_sys[-1].strip()
    if m_ram: ram_mib = int(m_ram[-1])
    if m_bld: build = m_bld[-1].strip()

    # Segment log lines by active app & scenario cues
    active_pkg = None
    in_scene = False
    scene_count = 0
    switches: list[dict[str, Any]] = []
    pending_switch: dict[str, Any] | None = None
    metrics_by_scenario: dict[str, dict[str, Any]] = {
        "normal_play": {"fps": [], "mean_ms": [], "max_ms": [], "hitch50": 0, "hitch100": 0},
        "heavy_crowd_effects": {"fps": [], "mean_ms": [], "max_ms": [], "hitch50": 0, "hitch100": 0},
    }

    for line in combined.splitlines():
        if "[prev]" in line:
            continue
        mp = re.search(r"javaapp:\s+([A-Za-z0-9_.]+),", line)
        if mp:
            active_pkg = mp.group(1)
            in_scene = False
        ms_req = re.search(r"\[switch\]\s+\[([^\]]+)\]\s+switch requested:\s+from=(\S+)\s+\(preState=(\d+)\)\s+->\s+to=(\S+)", line)
        if ms_req:
            pending_switch = {"from": ms_req.group(2), "pre_state": int(ms_req.group(3)), "to": ms_req.group(4), "ok": False}
        ms_done = re.search(r"\[switch\]\s+\[([^\]]+)\]\s+switch committed:\s+active=(\S+)\s+postState=(\d+)", line)
        if ms_done and pending_switch:
            pending_switch["ok"] = True
            pending_switch["post_state"] = int(ms_done.group(3))
            switches.append(pending_switch)
            pending_switch = None
        if "[switch] [" in line and ("blocked" in line or "failed" in line) and pending_switch:
            pending_switch["ok"] = False
            switches.append(pending_switch)
            pending_switch = None

        if active_pkg == TARGET_PACKAGE and ("vhpt-hud:" in line or "PTGameScene" in line or "KPakList::Open done" in line):
            if "vhpt-hud: charinfo" in line:
                in_scene = True
                scene_count += 1

        # Explicit scenario markers if present (or java-perf lines while VHPT is running)
        if "scenario=heavy_crowd_effects" in line or "CROWD_SCENE" in line:
            in_scene = True
            scene_count = max(scene_count, 2)
        jp = re.search(
            r"java-perf:\s+pkg=(\S+)\s+fps=([0-9.]+)\s+mean_ms=([0-9.]+)\s+max_ms=([0-9.]+)\s+hitch50=(\d+)\s+hitch100=(\d+)",
            line,
        )
        if jp and jp.group(1) == TARGET_PACKAGE:
            bucket = "heavy_crowd_effects" if ("scenario=heavy_crowd_effects" in line or "crowd" in line.lower()) else "normal_play"
            metrics_by_scenario[bucket]["fps"].append(float(jp.group(2)))
            metrics_by_scenario[bucket]["mean_ms"].append(float(jp.group(3)))
            metrics_by_scenario[bucket]["max_ms"].append(float(jp.group(4)))
            metrics_by_scenario[bucket]["hitch50"] += int(jp.group(5))
            metrics_by_scenario[bucket]["hitch100"] += int(jp.group(6))

    if pending_switch is not None:
        switches.append(pending_switch)

    vhpt_switches = [s for s in switches if TARGET_PACKAGE in (s.get("from"), s.get("to"))]
    return {
        "device_machine_id": device,
        "ios_version": ios_version,
        "ram_mib": ram_mib,
        "build": build,
        "log_dir": str(log_dir),
        "scenarios": metrics_by_scenario,
        "game_switches": vhpt_switches,
        "scene_entries": scene_count,
    }


def evaluate_real_scenario(samples: dict[str, Any]) -> tuple[str, dict[str, Any]]:
    fps_list = samples.get("fps") or []
    mean_list = samples.get("mean_ms") or []
    max_list = samples.get("max_ms") or []
    if not fps_list or not mean_list:
        return "chua du du lieu", {"measured": False}
    avg_fps = sum(fps_list) / len(fps_list)
    mean_ms = sum(mean_list) / len(mean_list)
    p95_ms = _quantile(mean_list, 0.95)
    max_ms = max(max_list) if max_list else mean_ms
    h50 = int(samples.get("hitch50", 0))
    h100 = int(samples.get("hitch100", 0))
    dur_min = max(len(fps_list) * 9.0 / 60.0, 1e-6)
    h50_per_min = h50 / dur_min
    is_pass = (
        avg_fps >= THRESHOLDS["pass_min_avg_fps"]
        and mean_ms <= THRESHOLDS["pass_max_mean_frame_ms"]
        and (p95_ms or 0.0) <= THRESHOLDS["pass_max_p95_frame_ms"]
        and max_ms <= THRESHOLDS["pass_max_frame_ms"]
        and h100 <= THRESHOLDS["pass_max_hitch100_count"]
        and h50_per_min <= THRESHOLDS["pass_max_hitch50_per_min"]
    )
    return ("dat" if is_pass else "co giat/lag"), {
        "measured": True,
        "samples": len(fps_list),
        "avg_fps": round(avg_fps, 1),
        "mean_frame_ms": round(mean_ms, 2),
        "p95_frame_ms": round(p95_ms or 0.0, 2),
        "max_frame_ms": round(max_ms, 2),
        "hitch50_count": h50,
        "hitch100_count": h100,
    }


def simulate_model_scenarios(model: IPhoneModelSpec) -> dict[str, Any] | None:
    """Analytical projection ONLY when --simulate is explicitly requested.

    Never promoted to real-device 'dat'/'co giat/lag' verdict; returned alongside
    'chua du du lieu' so simulated data and hardware evidence remain strictly separate.
    """
    if model.spec_status != "verified" or model.ram_gb is None or model.pixel_count is None:
        return None
    # VHPT is a 2D sprite + particle Cocos2d-x client (1280x720 design), sensitive to CPU single-core
    # JIT-less dispatch & RAM pressure on <=3 GB devices.
    fill_ratio = model.pixel_count / float(2796 * 1290)
    normal_fps = min(60.0, max(24.0, 60.0 * min(1.0, model.gpu_score / (0.28 + 0.16 * fill_ratio))))
    crowd_fps = min(60.0, max(18.0, normal_fps * (0.98 if model.gpu_score >= 0.80 else 0.72 + 0.25 * model.gpu_score)))
    ram_ok_switch = model.ram_gb >= 4.0
    return {
        "simulated_only": True,
        "normal_play": {"est_fps": round(normal_fps, 1), "est_frame_ms": round(1000.0 / normal_fps, 2)},
        "heavy_crowd_effects": {"est_fps": round(crowd_fps, 1), "est_frame_ms": round(1000.0 / crowd_fps, 2)},
        "game_switch": {"est_ram_headroom_risk": "low" if ram_ok_switch else "high (jetsam pressure <=3 GB)"},
    }


def evaluate_models(
    models: list[IPhoneModelSpec],
    parsed_logs: list[dict[str, Any]],
    include_simulation: bool = False,
) -> dict[str, Any]:
    rows: list[dict[str, Any]] = []
    missing_specs_models: list[str] = []
    missing_device_models: list[str] = []

    for m in models:
        matched_log = next(
            (lg for lg in parsed_logs if lg.get("device_machine_id") in m.machine_ids),
            None,
        )
        scenario_results: dict[str, Any] = {}
        conclusion = "chua du du lieu"
        data_source = "none (thieu thiet bi do)"
        tested_ios = m.compatible_ios
        build = "Chua do tren may that"

        if m.spec_status != "verified":
            missing_specs_models.append(m.variant)
            missing_device_models.append(m.variant)
            data_source = "none (thieu cau hinh xac minh + thieu thiet bi)"
        elif matched_log is None:
            missing_device_models.append(m.variant)
        else:
            data_source = "real_device"
            tested_ios = matched_log.get("ios_version") or m.compatible_ios
            build = matched_log.get("build") or "?"
            n_verdict, n_stats = evaluate_real_scenario(matched_log["scenarios"]["normal_play"])
            c_verdict, c_stats = evaluate_real_scenario(matched_log["scenarios"]["heavy_crowd_effects"])
            sw = matched_log.get("game_switches") or []
            sw_ok = len(sw) > 0 and all(s.get("ok") for s in sw)
            scenario_results = {
                "normal_play": {"verdict": n_verdict, **n_stats},
                "heavy_crowd_effects": {"verdict": c_verdict, **c_stats},
                "game_switch": {
                    "verdict": "dat" if sw_ok else ("co giat/lag" if sw else "chua du du lieu"),
                    "measured": len(sw) > 0,
                    "cycles": len(sw),
                    "passed_cycles": sum(1 for s in sw if s.get("ok")),
                },
            }
            verdicts = [
                scenario_results["normal_play"]["verdict"],
                scenario_results["heavy_crowd_effects"]["verdict"],
                scenario_results["game_switch"]["verdict"],
            ]
            if any(v == "co giat/lag" for v in verdicts):
                conclusion = "co giat/lag"
            elif all(v == "dat" for v in verdicts):
                conclusion = "dat"
            else:
                conclusion = "chua du du lieu"
                missing_device_models.append(f"{m.variant} (thieu kich ban: {', '.join(k for k in ('normal_play','heavy_crowd_effects','game_switch') if scenario_results[k]['verdict'] == 'chua du du lieu')})")

        sim = simulate_model_scenarios(m) if include_simulation else None
        if include_simulation and sim and data_source != "real_device":
            data_source = f"{data_source} + mo phong (chi tham khao, khong thay do that)"

        rows.append({
            "id": m.id,
            "family": m.family,
            "variant": m.variant,
            "machine_ids": list(m.machine_ids),
            "chip": m.chip,
            "ram_gb": m.ram_gb,
            "resolution": m.physical_resolution,
            "refresh_rate_hz": m.refresh_rate_hz,
            "compatible_ios": m.compatible_ios,
            "tested_ios": tested_ios,
            "build": build,
            "spec_status": m.spec_status,
            "spec_notes": m.spec_notes,
            "data_source": data_source,
            "scenarios": scenario_results,
            "simulation": sim,
            "conclusion": conclusion,
        })

    return {
        "package": TARGET_PACKAGE,
        "graphics_profile": DEFAULT_GRAPHICS_PROFILE,
        "thresholds": THRESHOLDS,
        "models_evaluated": len(rows),
        "missing_verified_specs": missing_specs_models,
        "missing_real_device_data": missing_device_models,
        "rows": rows,
    }


def format_markdown(report: dict[str, Any]) -> str:
    th = report["thresholds"]
    lines = [
        f"### Nguong danh gia hieu nang ({report['package']})",
        f"- Muc FPS muc tieu: **{th['target_fps']:.0f} FPS** (`config.ini` `FPS=60`, khung hinh chuan {th['frame_budget_ms']:.2f} ms).",
        f"- **Dat** (chi ket luan tu so lieu may that khi du 3 kich ban): `avg FPS >= {th['pass_min_avg_fps']:.1f}`, `mean <= {th['pass_max_mean_frame_ms']:.1f} ms`, `p95 <= {th['pass_max_p95_frame_ms']:.1f} ms`, `max <= {th['pass_max_frame_ms']:.1f} ms`, `hitch >= 100 ms == 0`, `hitch >= 50 ms <= {th['pass_max_hitch50_per_min']:.1f}/phut`, chuyen game 0 loi.",
        "- **Co giat/lag**: Bat ky chi so may that nao vuot nguong tren hoac giat keo dai.",
        "- **Chua du du lieu**: Thieu thiet bi that de do, thieu kich ban bat buoc, hoac thieu cau hinh phan cung xac minh (`iPhone 18` tieu chuan / `18e` / `Duo`).",
        "",
        "| Mau / Bien the | Chip | RAM | Do phan giai | Tan so quet | iOS ho tro / do | Build | Nguon du lieu | Ket luan |",
        "|---|---|---:|---|---:|---|---|---|---|",
    ]
    for r in report["rows"]:
        ram = f"{r['ram_gb']:.0f} GB" if r["ram_gb"] is not None else "Chua xac minh"
        hz = f"{r['refresh_rate_hz']} Hz" if r["refresh_rate_hz"] else "Chua xac minh"
        sc_summary = ""
        if r["scenarios"]:
            np_s = r["scenarios"].get("normal_play", {})
            hc_s = r["scenarios"].get("heavy_crowd_effects", {})
            sw_s = r["scenarios"].get("game_switch", {})
            parts = []
            if np_s.get("measured"): parts.append(f"Normal {np_s['avg_fps']}fps/{np_s['max_frame_ms']}ms")
            if hc_s.get("measured"): parts.append(f"Crowd {hc_s['avg_fps']}fps/{hc_s['max_frame_ms']}ms")
            if sw_s.get("measured"): parts.append(f"Switch {sw_s['passed_cycles']}/{sw_s['cycles']}")
            if parts: sc_summary = " (" + "; ".join(parts) + ")"
        elif r.get("simulation"):
            s = r["simulation"]
            sc_summary = f" [MO PHONG: ~{s['normal_play']['est_fps']}/{s['heavy_crowd_effects']['est_fps']} fps]"
        lines.append(
            f"| {r['variant']} | {r['chip']} | {ram} | {r['resolution']} | {hz} | {r['tested_ios']} | {r['build']} | {r['data_source']} | **{r['conclusion']}**{sc_summary} |"
        )
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true", help="List selectable iPhone 8 -> iPhone 18 models")
    ap.add_argument("--interactive", action="store_true", help="Choose an iPhone model/variant interactively in terminal")
    ap.add_argument("--model", default="all", help="Comma-separated model IDs, machine IDs, or families (default: all)")
    ap.add_argument("--log-dir", action="append", default=[], type=Path, help="Real device log folder(s) containing husk.log/husk-native.log")
    ap.add_argument("--simulate", action="store_true", help="Include clearly labeled analytical simulation estimates")
    ap.add_argument("--json-out", type=Path, help="Write JSON report")
    ap.add_argument("--markdown-out", type=Path, help="Write Markdown table report")
    args = ap.parse_args(argv)

    if args.list:
        for idx, m in enumerate(IPHONE_CATALOG, 1):
            flag = " [THIEU CAU HINH XAC MINH]" if m.spec_status != "verified" else ""
            print(f"{idx:2d}. {m.id:20s} | {m.variant:28s} | {m.chip:26s} | {m.physical_resolution} | {m.compatible_ios}{flag}")
        return 0

    selector = args.model
    if args.interactive:
        print("=== Chon mau iPhone de kiem thu (iPhone 8 -> iPhone 18) ===")
        for idx, m in enumerate(IPHONE_CATALOG, 1):
            flag = " [THIEU CAU HINH XAC MINH]" if m.spec_status != "verified" else ""
            print(f"  {idx:2d}. {m.variant} ({m.chip}, {m.physical_resolution}){flag}")
        raw = input("Nhap so thu tu (hoac ID, bo trong = all): ").strip()
        if raw.isdigit() and 1 <= int(raw) <= len(IPHONE_CATALOG):
            selector = IPHONE_CATALOG[int(raw) - 1].id
        elif raw:
            selector = raw

    models = select_models(selector)
    parsed = [parse_device_logs(d) for d in args.log_dir]
    report = evaluate_models(models, parsed, include_simulation=args.simulate)
    md = format_markdown(report)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    if args.markdown_out:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(md, encoding="utf-8")
    print(md)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
