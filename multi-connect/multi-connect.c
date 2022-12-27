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

#define MAX_QTS 10240
static demi_qtoken_t    *qts;
static uint32_t        nqts;

void build_sockaddr(const char *const ip_str, const char *const port_str, struct sockaddr_in *const addr)
{
    int port = -1;

    sscanf(port_str, "%d", &port);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    assert(inet_pton(AF_INET, ip_str, &addr->sin_addr) == 1);
}

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

void do_connect(const struct sockaddr_in *saddr)
{
    int sockqd;
    demi_qtoken_t qt;
    assert(demi_socket(&sockqd, AF_INET, SOCK_STREAM, 0) == 0);
    assert(demi_connect(&qt, sockqd, (const struct sockaddr *)saddr, sizeof(struct sockaddr_in)) == 0);
    add_event(qt);
}

int main(int argc, char *const argv[])
{
    struct sockaddr_in saddr = {0};
    demi_qresult_t qr = {0};
    int err = 0;
    int offset = 0;
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

    build_sockaddr(argv[1], argv[2], &saddr);

    do_connect(&saddr);
    do_connect(&saddr);

    while (1) {
        err = demi_wait_any(&qr, &offset, qts, nqts, &timeout);

        if (err) {
            assert(err == ETIMEDOUT);
            continue;
        }

        remove_event(offset);

        switch (qr.qr_opcode) {
        case DEMI_OPC_CONNECT:
            printf("connected\n");
            break;

        default:
            printf("unexpected opcode: %d\n", qr.qr_opcode);
        }
    }

    return 0;
}

