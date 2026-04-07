# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT
#
# Install this directory under nuttx-apps (e.g. apps/examples/ul_ecat_scan) or set
# APPDIR and build from an out-of-tree apps tree. UL_ECAT_ROOT must point to the
# ul_ecat repository root (defaults: three levels up from this Makefile).

include $(APPDIR)/Make.defs

# Repository root containing include/ and src/
UL_ECAT_ROOT ?= $(realpath $(CURDIR)/../../..)
include $(UL_ECAT_ROOT)/nuttx/Make.defs

PROGNAME = ul_ecat_scan
PRIORITY = SCHED_PRIORITY_DEFAULT
STACKSIZE = 8192
MODULE = $(CONFIG_EXAMPLES_UL_ECAT_SCAN)

MAINSRC = ul_ecat_scan_main.c

CSRCS += $(UL_ECAT_LIB_SRCS)

CFLAGS += -I$(UL_ECAT_INCDIR)

include $(APPDIR)/Application.mk
