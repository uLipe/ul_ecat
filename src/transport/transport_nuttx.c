/**
 * @file transport_nuttx.c
 * @brief NuttX AF_PACKET raw socket for EtherCAT L2 (see NuttX net/pkt documentation).
 */

#include "ul_ecat_transport.h"
#include "ul_ecat_frame.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

int ul_ecat_transport_open(const char *ifname)
{
    int s;
    struct ifreq ifr;
    struct sockaddr_ll sll;

    if (!ifname) {
        return -1;
    }

    s = socket(AF_PACKET, SOCK_RAW, htons(UL_ECAT_ETHERTYPE));
    if (s < 0) {
        perror("ul_ecat_transport_open: socket");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ul_ecat_transport_open: ioctl SIOCGIFINDEX");
        close(s);
        return -1;
    }

    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(UL_ECAT_ETHERTYPE);

    if (bind(s, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("ul_ecat_transport_open: bind");
        close(s);
        return -1;
    }

    return s;
}

void ul_ecat_transport_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

ssize_t ul_ecat_transport_send(int fd, const void *buf, size_t len, int flags)
{
    return send(fd, buf, len, flags);
}

ssize_t ul_ecat_transport_recv(int fd, void *buf, size_t cap, int flags)
{
    return recv(fd, buf, cap, flags);
}

int ul_ecat_transport_wait_readable(int fd, int timeout_ms)
{
    struct timeval tv;
    fd_set rfds;
    int r;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    r = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (r < 0) {
        return -1;
    }
    return r > 0 ? 1 : 0;
}
