#!/bin/bash
# Builds the Mac harness for src/app/Husk/WebApp.swift.
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
swiftc -O -swift-version 5 -o "${1:-/tmp/webapp-test}" "$R/tools/webapp/main.swift" "$R/src/app/Husk/WebApp.swift" -framework WebKit -framework AppKit
echo "built ${1:-/tmp/webapp-test}"
