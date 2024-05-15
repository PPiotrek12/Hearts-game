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
#include <fcntl.h>
#include <algorithm>
#include <chrono>
#include <sys/time.h>

#include "common.h"
#include "messages.h"
#include "err.h"
#include "game_server.h"

using namespace std;

const int MAX_CLIENTS = 100;
const int QUEUE_LENGTH = 50;

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

// Wrapper of pollfd structure with timeout in milliseconds.
struct Timeout_fd {
    int fd;
    int timeout; // In milliseconds, -1 if no timeout.
    int revents;
};

// Structure for listening on multiple file descriptors with timeouts. It holds a vector of clients
// and a file descriptor for accepting new connections. It has a method wrapped_poll that is a
// poll which ends after the shortest timeout of all clients. It updates timeouts and revents.
struct Listener {
    vector <Timeout_fd> clients;
    Timeout_fd accepts;
    void wrapped_poll() {
        pollfd fds[clients.size() + 1];
        fds[0] = {.fd = accepts.fd, .events = POLLIN, .revents = 0};
        int min_timeout = -1;
        for (int i = 0; i < (int)clients.size(); i++) {
            fds[i + 1] = {.fd = clients[i].fd, .events = POLLIN, .revents = 0};
            if (clients[i].timeout >= 0) {
                if (min_timeout == -1) min_timeout = clients[i].timeout;
                else min_timeout = min(min_timeout, clients[i].timeout);
            }
        }
        for (int i = 0; i < (int)clients.size() + 1; i++) { 
            if (fds[i].fd != -1 && fcntl(fds[i].fd, F_SETFL, O_NONBLOCK) < 0) // TODO: czy to potrzebne?
                syserr("fcntl"); // Set non-blocking mode.
            fds[i].revents = 0;
        }

        // Start measuring time.
        auto beg_time = chrono::system_clock::now();

        cout<< min_timeout << endl;
        int poll_status = poll(fds, clients.size() + 1, min_timeout);
        if (poll_status < 0) syserr("poll");

        // End measuring time and calculate duration in milliseconds.
        auto end_time = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = end_time - beg_time;
        int milliseconds = elapsed_seconds.count() * 1000;

        // Update timeouts and revents.
        accepts.revents = fds[0].revents;
        for (int i = 0; i < (int)clients.size(); i++) {
            if (clients[i].timeout != -1) clients[i].timeout -= milliseconds;
            clients[i].revents = fds[i + 1].revents;
        }
    }
};

void main_server_loop(int socket_fd, Game_scenario game_scenario, int timeout) {
    shared_ptr<Game_stage_server> game = make_shared<Game_stage_server>();

    // Create and fill Listener structure.
    Listener listener;
    listener.accepts = {.fd = socket_fd, .timeout = -1, .revents = 0};
    for (int i = 0; i < 4; i++)
        listener.clients.push_back({.fd = -1, .timeout = -1, .revents = 0});

    while (true) {
        listener.wrapped_poll();

        if (listener.accepts.revents & (POLLIN | POLLERR)) { // New client is connecting.
            sockaddr_in client_address;
            socklen_t client_address_len = sizeof(client_address);
            int client_fd = accept(listener.accepts.fd, (sockaddr*)&client_address, 
                                                                    &client_address_len);
            if (client_fd < 0) syserr("accept");

            Timeout_fd new_client = {.fd = client_fd, .timeout = timeout * 1000, .revents = 0};
            listener.clients.push_back(new_client);
        }
        for (int i = 4; i < (int)listener.clients.size(); i++) {
            if (listener.clients[i].revents & (POLLIN | POLLERR)) { // Client message.
                message mess = read_message(listener.clients[i].fd);
                if (mess.closed_connection) {
                    close(listener.clients[i].fd);
                    listener.clients.erase(listener.clients.begin() + i);
                }
            }
        }
    }
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

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // Czy tutaj AF_INET jest okej?
    if (socket_fd < 0) syserr("socket");

    if (bind(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) < 0) syserr("bind");
    if (listen(socket_fd, QUEUE_LENGTH) < 0) syserr("listen");

    main_server_loop(socket_fd, game_scenario, timeout);
}