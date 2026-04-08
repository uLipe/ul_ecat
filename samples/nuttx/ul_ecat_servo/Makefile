# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT
#
# NuttX app: EtherCAT slave + LAN9252 + servo PDO sample.
# Requires CONFIG_UL_ECAT_SLAVE=y, CONFIG_SPI=y, and your board SPI driver (SPI0).

include $(APPDIR)/Make.defs

UL_ECAT_ROOT ?= $(realpath $(CURDIR)/../../..)
include $(UL_ECAT_ROOT)/nuttx/Make.defs.slave_lan9252

PROGNAME = ul_ecat_servo
PRIORITY = SCHED_PRIORITY_DEFAULT
STACKSIZE = 8192
MODULE = $(CONFIG_EXAMPLES_UL_ECAT_SERVO)

MAINSRC = ul_ecat_servo_main.c

CFLAGS += -I$(UL_ECAT_INCDIR)
CFLAGS += -I$(UL_ECAT_ROOT)/samples/common

include $(APPDIR)/Application.mk
