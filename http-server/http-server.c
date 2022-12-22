// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// This should come first.
// Glibc macro to expose definitions corresponding to the POSIX.1-2008 base specification.
// See https://man7.org/linux/man-pages/man7/feature_test_macros.7.html.
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <demi/libos.h>
#include <demi/sga.h>
#include <demi/wait.h>
#include <string.h>
#include <errno.h>


#include <arpa/inet.h>
#include <sys/socket.h>

void build_sockaddr(const char *const ip_str, const char *const port_str, struct sockaddr_in *const addr)
{
    int port = -1;

    sscanf(port_str, "%d", &port);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    assert(inet_pton(AF_INET, ip_str, &addr->sin_addr) == 1);
}

#define MAX_QTS 10240
static demi_qtoken_t    *qts;
static uint32_t        nqts;
static const char *resp = "HTTP/1.1 204 No Content\r\nServer: demikernel\r\nContent-Length: 0\r\n\r\n";

void add_event(demi_qtoken_t qt)
{
    assert(nqts < MAX_QTS);
    qts[nqts] = qt;
    nqts++;
}

void remove_event(int i)
{
    assert(nqts > 0);
    nqts--;
    qts[i] = qts[nqts];
}

int main(int argc, char *const argv[])
{
    struct sockaddr_in saddr = {0};
    int sockqd = -1;
    demi_qtoken_t qt = -1;
    demi_qresult_t qr = {0};
    int err = 0;
    int offset = 0;
    uint32_t i;
    uint32_t len;
    char *buf;
    demi_sgarray_t resp_buf;
    struct timespec timeout = {
        .tv_sec = 1,
        .tv_nsec = 0
    };

    assert(demi_init(argc, argv) == 0);

    qts = malloc(sizeof(demi_qtoken_t) * MAX_QTS);
    if (!qts) {
        printf("fail to alloc qts\n");
        return 1;
    }
    nqts = 0;

    len = strlen(resp);
    printf("resp len: %d\n", len);

    resp_buf = demi_sgaalloc(sizeof(char) * len);
    if (resp_buf.sga_numsegs == 0) {
        printf("fail to alloc resp buf\n");
        return 1;
    }
    memcpy(resp_buf.sga_segs[0].sgaseg_buf, resp, sizeof(char) * len);
    resp_buf.sga_segs[0].sgaseg_len = sizeof(char) * len;

    build_sockaddr(argv[1], argv[2], &saddr);

    assert(demi_socket(&sockqd, AF_INET, SOCK_STREAM, 0) == 0);
    assert(demi_bind(sockqd, (const struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == 0);
    assert(demi_listen(sockqd, 10240) == 0);

    assert(demi_accept(&qt, sockqd) == 0);
    add_event(qt);

    while (1) {
        err = demi_wait_any(&qr, &offset, qts, nqts, &timeout);

        if (err) {
            if (err != ETIMEDOUT) {
                printf("fail to wait: %d\n", err);
            }
            continue;
        }

        remove_event(offset);

        switch (qr.qr_opcode) {
        case DEMI_OPC_PUSH:
            // wait for next request
            err = demi_pop(&qt, qr.qr_qd);

            if (err) {
                printf("fail to pop qd: %d due to: %d\n",
                        qr.qr_qd, err);
                continue;
            }

            add_event(qt);

            break;

        case DEMI_OPC_POP:
            len = qr.qr_value.sga.sga_segs[0].sgaseg_len;
            if (len == 0) {
                // conn ends
                continue;
            }

            i = 0;
            while (i < len) {
                buf = (char *)qr.qr_value.sga.sga_segs[0].sgaseg_buf + i;

                if (strncmp("\r\n\r\n", buf, 4) == 0) {
                    break;
                } else {
                    i++;
                }
            }

            demi_sgafree(&qr.qr_value.sga);

            if (i >= len) {
                err = demi_pop(&qt, qr.qr_qd);

                if (err) {
                    printf("fail to pop qd: %d due to: %d\n",
                            qr.qr_qd, err);
                    continue;
                }

                add_event(qt);

            } else {
                // request end

                err = demi_push(&qt, qr.qr_qd, &resp_buf);

                if (err) {
                    printf("fail to push qd: %d due to: %d\n",
                            qr.qr_qd, err);
                    continue;
                }

                add_event(qt);
            }

            break;

        case DEMI_OPC_ACCEPT:
            err = demi_pop(&qt, qr.qr_value.ares.qd);

            if (err) {
                printf("fail to pop accepted qd: %d due to: %d\n",
                        qr.qr_value.ares.qd, err);
                continue;
            }

            add_event(qt);

            err = demi_accept(&qt, sockqd);

            if (err) {
                printf("fail to accept again: %d\n", err);
                return 1;
            }

            add_event(qt);

            break;

        default:
            printf("unexpected opcode: %d\n", qr.qr_opcode);
        }


    }

    return 0;
}
