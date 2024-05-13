#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <vector>
#include <cstdio>
#include <poll.h>
#include <signal.h>

#include "common.h"
#include "messages.h"
#include "err.h"
#include "game_server.h"

using namespace std;

void parse_arguments(int argc, char* argv[], uint16_t *port, bool *wasPortSet, string *file, int *timeout) {
    *timeout = 5;
    bool wasFileSet = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-p") {
            if (i + 1 >= argc) fatal("missing argument for -p");
            else {
                *port = read_port(argv[i + 1]);
                *wasPortSet = true;
                i++;
            }
        } 
        else if (arg == "-f") {
            if (i + 1 >= argc) fatal("missing argument for -f");
            else {
                *file = argv[i + 1];
                wasFileSet = true;
                i++;
            }
        }
        else if (arg == "-t") {
            if (i + 1 >= argc) fatal("missing argument for -t");
            else {
                *timeout = stoi(argv[i + 1]);
                i++;
            }
        }
        else fatal("unknown argument");
    }
    if (!wasFileSet) fatal("missing -f argument");
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    uint16_t port;
    string file;
    int timeout;
    bool wasPortSet = false;
    parse_arguments(argc, argv, &port, &wasPortSet, &file, &timeout);
    Game_scenario game_scenario;
    game_scenario.parse(file);

    sockaddr_in server_address;
    server_address.sin_family = AF_UNSPEC;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (wasPortSet) server_address.sin_port = htons(port);

    int socket_fd = socket(AF_UNSPEC, SOCK_STREAM, 0);
    if (socket_fd < 0) syserr("socket");

    if (bind(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) < 0) syserr("bind");
}