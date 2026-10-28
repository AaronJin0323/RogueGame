#!/bin/bash
# Builds DusktracerEditor Mac Development. Runs ON the Mac. Override UE_ROOT to use another engine install.
set -euo pipefail
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ ! -x "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" ]; then
	echo "NOT RUN: UE 5.8 not found at $UE_ROOT" >&2
	exit 2
fi
if ! xcodebuild -version >/dev/null 2>&1; then
	echo "NOT RUN: Xcode is not installed or not selected (xcode-select -s /Applications/Xcode.app)" >&2
	exit 2
fi

"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DusktracerEditor Mac Development -Project="$ROOT/Dusktracer.uproject" -WaitMutex
