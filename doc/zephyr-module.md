# Zephyr module: `ul_ecat`

This repository is a [Zephyr module](https://docs.zephyrproject.org/latest/develop/modules.html): metadata lives in [`zephyr/module.yml`](../zephyr/module.yml), build logic in [`zephyr/CMakeLists.txt`](../zephyr/CMakeLists.txt), and Kconfig in [`zephyr/Kconfig`](../zephyr/Kconfig).

## Quick start

1. Install Zephyr and `west`; choose a board with Ethernet (e.g. `native_sim/native/64` for a host-tap setup).
2. `export ZEPHYR_EXTRA_MODULES=/absolute/path/to/ul_ecat` (repository root containing `zephyr/module.yml`).
3. `west build -b <board> -p always /path/to/ul_ecat/samples/zephyr/ul_ecat_scan` then `west build -t run` (or flash on hardware).
4. Enable `CONFIG_UL_ECAT=y` and `CONFIG_UL_ECAT_MASTER=y` (already set in the sample [`prj.conf`](../samples/zephyr/ul_ecat_scan/prj.conf)). Match the interface string in [`main.c`](../samples/zephyr/ul_ecat_scan/src/main.c) to your network interface.

For a one-page overview with NuttX side by side, see [README.md § Quick start](../README.md#quick-start-zephyr-and-nuttx).

## Registering the module

Point `ZEPHYR_EXTRA_MODULES` at the **repository root** (the directory that contains `zephyr/module.yml`):

```bash
export ZEPHYR_EXTRA_MODULES=/path/to/ul_ecat
west build -b native_sim/native/64 /path/to/ul_ecat/samples/zephyr/ul_ecat_scan
```

The sample [`samples/zephyr/ul_ecat_scan`](../samples/zephyr/ul_ecat_scan) appends this repo to `ZEPHYR_EXTRA_MODULES` in its `CMakeLists.txt`, so you can also build from that directory without setting the variable (unless you need additional modules).

## Kconfig

| Option | Meaning |
|--------|---------|
| `CONFIG_UL_ECAT` | Enables the module library. You must also enable **`CONFIG_UL_ECAT_MASTER` and/or `CONFIG_UL_ECAT_SLAVE`**. |
| `CONFIG_UL_ECAT_MASTER` | Builds the master core (scan, AL queue, DC hooks). Requires networking; implies `CONFIG_NET_SOCKETS`, `CONFIG_NET_SOCKETS_PACKET`, and basic IPv4/IPv6 stack symbols as needed by typical Ethernet setups. |
| `CONFIG_UL_ECAT_SLAVE` | Builds the ESC/slave stack and LAN9252 driver. **Selects `CONFIG_SPI`** (no Ethernet required). |
| `CONFIG_UL_ECAT_LAN9252_HAL` | Zephyr SPI HAL ([`ports/lan9252/zephyr/hal_zephyr.c`](../ports/lan9252/zephyr/hal_zephyr.c)): devicetree `/chosen` property **`ul-ecat-spi`**, `SYS_INIT` bind, manual CS. Default `y` when slave is enabled. |
| `CONFIG_UL_ECAT_TRANSPORT_NETDEV` | **TX path:** send frames with `net_if_queue_tx()` / `net_pkt` instead of `zsock_send()`. RX still uses the packet socket opened on the same interface. |

Slave + LAN9252 on RTOS is described in [`doc/rtos-lan9252.md`](rtos-lan9252.md). Reference application: [`samples/zephyr/ul_ecat_servo`](../samples/zephyr/ul_ecat_servo/).

Packet sockets bind to a specific EtherCAT ethertype (`0x88A4`). Other traffic on the same interface may still be visible to the stack; see Zephyr networking docs.

## Transport modes

- **Default (`CONFIG_UL_ECAT_TRANSPORT_NETDEV=n`):** [`transport_zephyr.c`](../src/transport/transport_zephyr.c) — BSD-style packet socket using `zsock_*` APIs (`AF_PACKET` / `SOCK_RAW`).
- **Optional (`CONFIG_UL_ECAT_TRANSPORT_NETDEV=y`):** [`transport_zephyr_netdev.c`](../src/transport/transport_zephyr_netdev.c) — transmits L2 frames via `net_if_queue_tx()`; receive/select behavior is unchanged (socket).

## Reference build: scan sample

Application: [`samples/zephyr/ul_ecat_scan`](../samples/zephyr/ul_ecat_scan).

```bash
export ZEPHYR_EXTRA_MODULES=/path/to/ul_ecat
west build -b native_sim/native/64 -p always /path/to/ul_ecat/samples/zephyr/ul_ecat_scan
west build -t run
```

Requirements depend on the board:

- **native_sim** with `CONFIG_ETH_NATIVE_POSIX=y` (see `prj.conf`): host networking setup matches [Zephyr’s Ethernet on native_sim](https://docs.zephyrproject.org/latest/connectivity/networking/networking_with_host.html). Adjust interface name in `src/main.c` (`UL_ECAT_SAMPLE_IFACE_NAME`, default `eth0`) or pass `-DUL_ECAT_SAMPLE_IFACE_NAME=\"…\"` at build time.
- **Hardware boards:** enable the appropriate `CONFIG_ETH_*` / driver options; set the interface name to match `net_if` (often `eth0`).

## OS abstraction

- [`include/ul_ecat_osal.h`](../include/ul_ecat_osal.h) — mutexes, worker thread, monotonic time, event wait/signal.
- Linux: [`src/osal/osal_linux.c`](../src/osal/osal_linux.c) (used by the standalone CMake target).
- Zephyr: [`src/osal/osal_zephyr.c`](../src/osal/osal_zephyr.c) (`k_mutex`, `k_thread`, `k_condvar`).

## Versioning

Fix a Zephyr **LTS** or release in your product manifest; packet socket and `net_if` APIs evolve between releases. If something fails to link, compare the sample with [`samples/net/sockets/packet`](https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/net/sockets/packet) in the same Zephyr revision.
