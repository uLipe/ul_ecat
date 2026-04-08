# NuttX integration: `ul_ecat`

This repository adds NuttX-specific OSAL and transport next to the portable core, mirroring the Zephyr layout ([`zephyr/`](../zephyr/), [`doc/zephyr-module.md`](zephyr-module.md)).

**Reference NuttX version:** Apache NuttX 12.x / `master` (2025–2026). Validate `CONFIG_*` names with `menuconfig` on your exact revision.

## Quick start

1. Clone **apache/nuttx** and **apache/nuttx-apps** (or your vendor fork). Configure a board with `CONFIG_NET=y`, Ethernet, and packet sockets (`CONFIG_NET_PKT=y`; see [`defconfig.snippet`](../samples/nuttx/ul_ecat_scan/defconfig.snippet)).
2. In `nuttx-apps` **`apps/Kconfig`**, `source` the absolute paths to [`nuttx/Kconfig`](../nuttx/Kconfig) and [`samples/nuttx/ul_ecat_scan/Kconfig`](../samples/nuttx/ul_ecat_scan/Kconfig).
3. Copy or symlink [`samples/nuttx/ul_ecat_scan`](../samples/nuttx/ul_ecat_scan) into `apps/examples/ul_ecat_scan` (or keep it in the `ul_ecat` tree and set **`UL_ECAT_ROOT`** in the app `Makefile`).
4. Run `menuconfig` / `make menuconfig` and enable `CONFIG_UL_ECAT`, `CONFIG_UL_ECAT_MASTER`, and `CONFIG_EXAMPLES_UL_ECAT_SCAN`.
5. Build from the NuttX tree (`./tools/configure.sh …`, `make`), then run `ul_ecat_scan [interface]` from NSH (default interface name `eth0` if omitted in the sample; see [`ul_ecat_scan_main.c`](../samples/nuttx/ul_ecat_scan/ul_ecat_scan_main.c)).

For a one-page overview with Zephyr side by side, see [README.md § Quick start](../README.md#quick-start-zephyr-and-nuttx).

## Layout

| Path | Role |
|------|------|
| [`nuttx/Kconfig`](../nuttx/Kconfig) | `CONFIG_UL_ECAT`, `CONFIG_UL_ECAT_MASTER` (needs `NET`), `CONFIG_UL_ECAT_SLAVE` (selects SPI) |
| [`nuttx/Make.defs`](../nuttx/Make.defs) | Sets `UL_ECAT_LIB_SRCS` and `UL_ECAT_INCDIR` when `UL_ECAT_ROOT` points at this repo |
| [`nuttx/Make.defs.slave_lan9252`](../nuttx/Make.defs.slave_lan9252) | Slave + LAN9252 + [`controllers/lan9252/ports/nuttx/hal_nuttx.c`](../controllers/lan9252/ports/nuttx/hal_nuttx.c) |
| [`nuttx/ul_ecat_sources.cmake`](../nuttx/ul_ecat_sources.cmake) | `UL_ECAT_NUTTX_SOURCES` / `UL_ECAT_NUTTX_INCLUDE_DIR` for CMake-based NuttX builds |
| [`nuttx/ul_ecat_slave_lan9252_sources.cmake`](../nuttx/ul_ecat_slave_lan9252_sources.cmake) | Slave + LAN9252 sources for CMake |
| [`nuttx/CMakeLists.txt`](../nuttx/CMakeLists.txt) | Optional `ul_ecat_nuttx` and `ul_ecat_nuttx_slave_lan9252` INTERFACE libraries |
| [`src/osal/osal_nuttx.c`](../src/osal/osal_nuttx.c) | pthread mutex/cond, worker thread, monotonic clock (no `mlockall`) |
| [`src/transport/transport_nuttx.c`](../src/transport/transport_nuttx.c) | `AF_PACKET` / `SOCK_RAW`, `struct sockaddr_ll` from [`netpacket/packet.h`](https://github.com/apache/nuttx/blob/master/include/netpacket/packet.h), `ioctl(SIOCGIFINDEX)` |
| [`samples/nuttx/ul_ecat_scan/`](../samples/nuttx/ul_ecat_scan/) | Example app: scan + print slaves |
| [`samples/nuttx/ul_ecat_servo/`](../samples/nuttx/ul_ecat_servo/) | Example app: slave + LAN9252 SPI0 hook |

RTOS LAN9252 details: [`doc/rtos-lan9252.md`](rtos-lan9252.md).

## Kconfig wiring (nuttx-apps)

1. Copy or symlink [`samples/nuttx/ul_ecat_scan`](../samples/nuttx/ul_ecat_scan) into your `apps` tree (e.g. `apps/examples/ul_ecat_scan`).
2. In `apps/Kconfig`, **source** the module and the example (adjust the path to your clone):

```kconfig
source "/path/to/ul_ecat/nuttx/Kconfig"
source "/path/to/ul_ecat/samples/nuttx/ul_ecat_scan/Kconfig"
```

3. Run `menuconfig` and enable:

- `CONFIG_UL_ECAT=y`
- `CONFIG_UL_ECAT_MASTER=y`
- `CONFIG_EXAMPLES_UL_ECAT_SCAN=y`

4. Merge networking options from [`samples/nuttx/ul_ecat_scan/defconfig.snippet`](../samples/nuttx/ul_ecat_scan/defconfig.snippet) as needed (`CONFIG_NET`, `CONFIG_NET_PKT`, Ethernet driver for your board).

## `UL_ECAT_ROOT` in the application Makefile

If the example lives **inside** this repository (`samples/nuttx/ul_ecat_scan`), the default `$(realpath $(CURDIR)/../../..)` is the repository root.

If you moved the app under `apps/examples/ul_ecat_scan`, set **`UL_ECAT_ROOT`** explicitly to the `ul_ecat` repository root before including [`nuttx/Make.defs`](../nuttx/Make.defs):

```makefile
UL_ECAT_ROOT := /absolute/path/to/ul_ecat
include $(UL_ECAT_ROOT)/nuttx/Make.defs
```

## CMake (optional)

```cmake
set(UL_ECAT_ROOT "/path/to/ul_ecat" CACHE PATH "")
include(${UL_ECAT_ROOT}/nuttx/ul_ecat_sources.cmake)
target_sources(myapp PRIVATE ${UL_ECAT_NUTTX_SOURCES})
target_include_directories(myapp PRIVATE ${UL_ECAT_NUTTX_INCLUDE_DIR})
```

## Running the scan example

Default interface is `eth0` (or `UL_ECAT_SAMPLE_IFACE_NAME` at compile time). Pass the interface name as the first argument to override:

```text
nsh> ul_ecat_scan
nsh> ul_ecat_scan eth0
```

Raw / packet access requires appropriate privileges and a driver that supports packet sockets; see NuttX [raw packet documentation](https://nuttx.apache.org/docs/latest/components/net/pkt.html).

## Limitations

- Packet sockets see L2 traffic on the selected interface; behavior depends on the NuttX version and driver.
- `struct ifreq` / `ifr_ifindex` must match your NuttX `net/if.h`; if `ioctl(SIOCGIFINDEX)` fails on your port, adjust [`transport_nuttx.c`](../src/transport/transport_nuttx.c) using `if_nametoindex()` or board-specific APIs.

## Host CMake in this repo

The top-level [`CMakeLists.txt`](../CMakeLists.txt) still builds the **Linux** stack (`osal_linux` + `transport_linux`) only. NuttX sources are not built by default.
