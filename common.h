#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

uint16_t read_port(char const *string);
struct sockaddr_in get_server_address(char const *host, uint16_t port);
ssize_t readn(int fd, char *vptr, size_t n);
ssize_t writen(int fd, char *vptr, size_t n);

#endif
