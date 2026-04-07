/**
 * @file ul_ecat_slave_harness.c
 * @brief TCP loopback test server: length-prefixed raw Ethernet frames in/out, libul_ecat_slave reply.
 *
 * Framing (big-endian uint32 length N, then N bytes): same on TX and RX.
 * Identity defaults come from generated ul_ecat_slave_tables.c (see scripts/gen_slave_data.py).
 */

#include "ul_ecat_slave.h"
#include "ul_ecat_slave_tables.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_full(int fd, void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, p + off, n - off);
        if (r <= 0) {
            return -1;
        }
        off += (size_t)r;
    }
    return 0;
}

static int write_full(int fd, const void *buf, size_t n)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);
        if (w <= 0) {
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [-p PORT]\n", argv0);
    fprintf(stderr, "  Listen on 127.0.0.1:PORT (default %u) and serve one EtherCAT-over-TCP client.\n",
            9234u);
}

int main(int argc, char **argv)
{
    uint16_t port = 9234u;
    int c;
    while ((c = getopt(argc, argv, "p:h")) != -1) {
        switch (c) {
        case 'p':
            port = (uint16_t)strtoul(optarg, NULL, 10);
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    ul_ecat_slave_t slave;
    uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    ul_ecat_slave_init(&slave, mac, &ul_ecat_generated_slave_identity);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }
    int opt = 1;
    (void)setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    if (listen(srv, 1) != 0) {
        perror("listen");
        close(srv);
        return 1;
    }

    fprintf(stderr, "ul_ecat_slave_harness: listening 127.0.0.1:%u\n", (unsigned)port);
    fflush(stderr);

    for (;;) {
        int conn = accept(srv, NULL, NULL);
        if (conn < 0) {
            perror("accept");
            continue;
        }

        uint8_t rx[2048];
        uint8_t tx[2048];

        for (;;) {
            uint32_t nbe = 0;
            if (read_full(conn, &nbe, sizeof(nbe)) != 0) {
                break;
            }
            uint32_t n = ntohl(nbe);
            if (n == 0u || n > sizeof(rx)) {
                uint32_t z = htonl(0u);
                (void)write_full(conn, &z, sizeof(z));
                break;
            }
            if (read_full(conn, rx, n) != 0) {
                break;
            }

            size_t tx_len = 0;
            if (ul_ecat_slave_process_ethernet(&slave, rx, n, tx, sizeof(tx), &tx_len) != 0) {
                uint32_t z = htonl(0u);
                if (write_full(conn, &z, sizeof(z)) != 0) {
                    break;
                }
                continue;
            }

            uint32_t outbe = htonl((uint32_t)tx_len);
            if (write_full(conn, &outbe, sizeof(outbe)) != 0) {
                break;
            }
            if (tx_len > 0u && write_full(conn, tx, tx_len) != 0) {
                break;
            }
        }
        close(conn);
    }

    close(srv);
    return 0;
}
