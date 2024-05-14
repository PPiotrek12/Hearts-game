#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

uint16_t read_port(char const *string);
int get_server_address(char const *host, uint16_t port, bool useIPv4, bool useIPv6,
                            sockaddr_in *address4, sockaddr_in6 *address6);
ssize_t readn(int fd, char *vptr, size_t n);
ssize_t writen(int fd, char *vptr, size_t n);

#endif
