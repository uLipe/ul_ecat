/**
 * @file transport_zephyr_netdev.c
 * @brief Zephyr transport: packet socket for RX/wait; TX via net_if queue (L2 frame buffer).
 *
 * Use when CONFIG_UL_ECAT_TRANSPORT_NETDEV=y (mutually exclusive with transport_zephyr.c).
 */

#include "ul_ecat_transport.h"
#include "ul_ecat_frame.h"

#include <errno.h>
#include <string.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/socket.h>

static struct net_if *g_netif;

int ul_ecat_transport_open(const char *ifname)
{
	if (ifname == NULL) {
		return -1;
	}

	int ifindex = net_if_get_by_name(ifname);

	if (ifindex < 0) {
		return -1;
	}

	g_netif = net_if_get_by_index(ifindex);
	if (g_netif == NULL) {
		return -1;
	}

	int s = zsock_socket(AF_PACKET, SOCK_RAW, htons(UL_ECAT_ETHERTYPE));

	if (s < 0) {
		g_netif = NULL;
		return -1;
	}

	struct sockaddr_ll bind_addr = {0};

	bind_addr.sll_family = AF_PACKET;
	bind_addr.sll_ifindex = ifindex;
	bind_addr.sll_protocol = htons(UL_ECAT_ETHERTYPE);

	if (zsock_bind(s, (const struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		(void)zsock_close(s);
		g_netif = NULL;
		return -1;
	}

	return s;
}

void ul_ecat_transport_close(int fd)
{
	g_netif = NULL;
	if (fd >= 0) {
		(void)zsock_close(fd);
	}
}

ssize_t ul_ecat_transport_send(int fd, const void *buf, size_t len, int flags)
{
	(void)fd;
	(void)flags;

	if (g_netif == NULL) {
		return -1;
	}

	struct net_pkt *pkt = net_pkt_alloc_with_buffer(g_netif, len, AF_UNSPEC, 0, K_MSEC(200));

	if (pkt == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (net_pkt_write(pkt, buf, len) != 0) {
		net_pkt_unref(pkt);
		errno = EIO;
		return -1;
	}

	net_if_queue_tx(g_netif, pkt);
	return (ssize_t)len;
}

ssize_t ul_ecat_transport_recv(int fd, void *buf, size_t cap, int flags)
{
	return zsock_recv(fd, buf, cap, flags);
}

int ul_ecat_transport_wait_readable(int fd, int timeout_ms)
{
	struct zsock_pollfd pfd = {
		.fd = fd,
		.events = ZSOCK_POLLIN,
	};

	int ret = zsock_poll(&pfd, 1, timeout_ms);

	if (ret < 0) {
		return -1;
	}
	return ret > 0 ? 1 : 0;
}
