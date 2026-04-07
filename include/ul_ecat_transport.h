/**
 * @file ul_ecat_transport.h
 * @brief Raw L2 transport for EtherCAT frames (platform-specific implementation).
 */

#ifndef UL_ECAT_TRANSPORT_H
#define UL_ECAT_TRANSPORT_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open a packet socket bound to @p ifname for EtherCAT ethertype (0x88A4).
 * @return socket handle (>=0) or -1 on error.
 */
int ul_ecat_transport_open(const char *ifname);

void ul_ecat_transport_close(int fd);

ssize_t ul_ecat_transport_send(int fd, const void *buf, size_t len, int flags);

ssize_t ul_ecat_transport_recv(int fd, void *buf, size_t cap, int flags);

/**
 * Block until the socket is readable or @p timeout_ms elapses.
 * @return 1 if readable, 0 on timeout, -1 on error.
 */
int ul_ecat_transport_wait_readable(int fd, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_TRANSPORT_H */
