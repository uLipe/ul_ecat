#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# NuttX: não compilamos um BSP completo no CI; só verificamos ficheiros referenciados.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

for f in \
	ports/lan9252/nuttx/hal_nuttx.c \
	ports/lan9252/nuttx/hal_nuttx.h \
	nuttx/Make.defs.slave_lan9252 \
	nuttx/ul_ecat_slave_lan9252_sources.cmake
do
	if [[ ! -f "$f" ]]; then
		echo "missing: $f" >&2
		exit 1
	fi
done

echo "OK: NuttX integration files present (full build needs a board tree)."
