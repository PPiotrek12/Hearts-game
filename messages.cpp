#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <istream>
#include <unistd.h>
#include <chrono>
#include <ctime>

#include "messages.h"
#include "common.h"
#include "err.h"

using namespace std;

// Function giving the local address of the socket.
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

// Function giving the peer address of the socket.
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

// Get the current time.
string get_current_time() {
    auto now = chrono::system_clock::now();
    time_t time = chrono::system_clock::to_time_t(now);
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000", localtime(&time));
    return string(buffer);
}

const int MAX_SIZE = 1000;
// Function reading a message from the socket - reads it into the buffer.
int read_message(int fd, string *buffer, bool is_auto_player) {
    char act[MAX_SIZE];
    int length = read(fd, act, MAX_SIZE);
    if (length < 0) syserr("read");
    if (length == 0) return 0;
    *buffer += string(act, length);
    string current_time = get_current_time();
    if (is_auto_player)
        cout << "[" << peer_address(fd) << "," << local_address(fd) 
             << "," << current_time << "] " << string(act, length); // TODO: czy tu nie powinno byc entera?
    return length;
}

message parse_message(string *buffer) {
    // Extract the first message from the buffer - up to the first "\r\n".
    string mess;
    for (int i = 0; i < (int)buffer->size(); i++) {
        if (i + 1 < (int)buffer->size() && (*buffer)[i] == '\r' && (*buffer)[i + 1] == '\n') {
            mess = buffer->substr(0, i);
            *buffer = buffer->substr(i + 2);
            break;
        }
    }
    message res;
    if (mess.empty()) { // There weren't any full messages in the buffer.
        res.empty = true;
        return res;
    }
    res.parse(mess);
    return res;
}

void send_message(int fd, message mess, bool is_auto_player) {
    string to_send = mess.to_message();
    if (writen(fd, (char*)to_send.c_str(), to_send.size()) < 0)
        syserr("write");
    string current_time = get_current_time();
    if (is_auto_player)
        cout << "[" << local_address(fd) << "," << peer_address(fd) 
             << "," << current_time << "] " << to_send; // TODO: czy tu nie powinno byc entera?
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