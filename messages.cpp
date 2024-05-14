#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "messages.h"
#include "common.h"
#include "err.h"

using namespace std;

string local_address(int fd) {
    struct sockaddr_storage local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(fd, (struct sockaddr *)&local_addr, &addr_len) == -1) syserr("getsockname");
    char local_ip[INET6_ADDRSTRLEN];
    uint16_t local_port;
    if (local_addr.ss_family == AF_INET) {
        struct sockaddr_in *local_addr_ipv4 = (struct sockaddr_in *)&local_addr;
        inet_ntop(AF_INET, &(local_addr_ipv4->sin_addr), local_ip, INET_ADDRSTRLEN);
        local_port = ntohs(local_addr_ipv4->sin_port);
    } else {
        struct sockaddr_in6 *local_addr_ipv6 = (struct sockaddr_in6 *)&local_addr;
        inet_ntop(AF_INET6, &(local_addr_ipv6->sin6_addr), local_ip, INET6_ADDRSTRLEN);
        local_port = ntohs(local_addr_ipv6->sin6_port);
    }
    string res = local_ip;
    res += ":";
    res += to_string(local_port);
    return res;
}

string peer_address(int fd) {
    struct sockaddr_storage peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    if (getpeername(fd, (struct sockaddr *)&peer_addr, &addr_len) == -1) syserr("getpeername");
    char peer_ip[INET6_ADDRSTRLEN];
    uint16_t peer_port;
    if (peer_addr.ss_family == AF_INET) {
        struct sockaddr_in *peer_addr_ipv4 = (struct sockaddr_in *)&peer_addr;
        inet_ntop(AF_INET, &(peer_addr_ipv4->sin_addr), peer_ip, INET_ADDRSTRLEN);
        peer_port = ntohs(peer_addr_ipv4->sin_port);
    } else {
        struct sockaddr_in6 *peer_addr_ipv6 = (struct sockaddr_in6 *)&peer_addr;
        inet_ntop(AF_INET6, &(peer_addr_ipv6->sin6_addr), peer_ip, INET6_ADDRSTRLEN);
        peer_port = ntohs(peer_addr_ipv6->sin6_port);
    }
    string res = peer_ip;
    res += ":";
    res += to_string(peer_port);
    return res;
}

message read_message(int fd, bool is_auto_player) {
    char act;
    string mess;
    do {
        int read = readn(fd, &act, 1);
        if (read < 0) {
            if (errno == EPIPE) {
                message res = {.closed_connection = true};
                return res;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                message res = {.timeout = true};
                return res;
            }
            syserr("read");
        }
            
        if (read == 0) {
            message res = {.closed_connection = true};
            return res;
        }
        mess += act;
    } while (act != '\n');
    if (is_auto_player)
        cout << "[" << peer_address(fd) << ", " << local_address(fd) << "] " << mess << "\n";
    mess = mess.substr(0, mess.size() - 2);
    message res;
    res.parse(mess);
    return res;
}

void send_message(int fd, message mess, bool is_auto_player) {
    string to_send = mess.to_message();
    if (writen(fd, (char*)to_send.c_str(), to_send.size()) < 0)
        syserr("write");
    if (is_auto_player)
        cout << "[" << local_address(fd) << ", " << peer_address(fd) << "] " << to_send << "\n";
}

int seat_to_int(char seat) {
    if (seat == 'N') return 0;
    if (seat == 'E') return 1;
    if (seat == 'S') return 2;
    if (seat == 'W') return 3;
    return -1;
}

char int_to_seat(int seat) {
    if (seat == 0) return 'N';
    if (seat == 1) return 'E';
    if (seat == 2) return 'S';
    if (seat == 3) return 'W';
    return 'X';
}