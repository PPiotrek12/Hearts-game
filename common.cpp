#include <sys/types.h>
#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "err.h"
#include "common.h"

using namespace std;

// Read n bytes from a descriptor. Use in place of read() when fd is a stream socket.
ssize_t readn(int fd, char *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;
        else if (nread == 0)
            break;
        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, char *vptr, size_t n){
    ssize_t nleft, nwritten;
    char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

int get_server_address(char const *host, uint16_t port, bool useIPv4, bool useIPv6,
                            sockaddr_in *address4, sockaddr_in6 *address6) {
    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    if (useIPv4)
        hints.ai_family = AF_INET; // IPv4
    else if (useIPv6)
        hints.ai_family = AF_INET6; // IPv6
    else
        hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *res;
    int errcode = getaddrinfo(host, NULL, &hints, &res);
    if (errcode != 0)
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    if (res->ai_family == AF_INET6) {
        address6->sin6_family = AF_INET6;
        address6->sin6_addr = ((struct sockaddr_in6 *) (res->ai_addr))->sin6_addr;
        address6->sin6_port = htons(port);
        freeaddrinfo(res);
        return AF_INET6;
    }
    address4->sin_family = AF_INET;
    address4->sin_addr.s_addr = ((struct sockaddr_in *) (res->ai_addr))->sin_addr.s_addr;
    address4->sin_port = htons(port);
    freeaddrinfo(res);
    return AF_INET;
}