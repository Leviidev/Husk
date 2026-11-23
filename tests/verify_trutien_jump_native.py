#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compatibility entry point for the separate plain-Jump native checks."""
from trutien_native_harness import (
    BASE, CONFIG, CONTEXT, INTERMEDIATE, LAYOUT, STOP, STACK,
    NativeHUD, double_bits, rect, overlap,
)
from verify_trutien_plain_jump_native import main

if __name__ == '__main__':
    main()
