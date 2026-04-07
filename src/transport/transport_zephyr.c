/**
 * @file transport_zephyr.c
 * @brief Zephyr packet socket (AF_PACKET) for EtherCAT L2.
 */

#include "ul_ecat_transport.h"
#include "ul_ecat_frame.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/unistd.h>

int ul_ecat_transport_open(const char *ifname)
{
	if (ifname == NULL) {
		return -1;
	}

	int ifindex = net_if_get_by_name(ifname);

	if (ifindex < 0) {
		return -1;
	}

	int s = zsock_socket(AF_PACKET, SOCK_RAW, htons(UL_ECAT_ETHERTYPE));

	if (s < 0) {
		return -1;
	}

	struct sockaddr_ll bind_addr = {0};

	bind_addr.sll_family = AF_PACKET;
	bind_addr.sll_ifindex = ifindex;
	bind_addr.sll_protocol = htons(UL_ECAT_ETHERTYPE);

	if (zsock_bind(s, (const struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		(void)zsock_close(s);
		return -1;
	}

	return s;
}

void ul_ecat_transport_close(int fd)
{
	if (fd >= 0) {
		(void)zsock_close(fd);
	}
}

ssize_t ul_ecat_transport_send(int fd, const void *buf, size_t len, int flags)
{
	return zsock_send(fd, buf, len, flags);
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
