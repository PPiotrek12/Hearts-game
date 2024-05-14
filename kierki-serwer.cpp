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

// Returns 0 if client was joined to the game, 1 if client was not joined to the game.
int join_client_to_game(pollfd *fds, shared_ptr<Game_stage_server> game, int i) {
    message mess = read_message(fds[i].fd); // TODO: timeout
    if (mess.is_iam) {
        if (seat_to_int(mess.iam.player) != -1 && !game->occupied[seat_to_int(mess.iam.player)]) {
            game->occupied[seat_to_int(mess.iam.player)] = true;
            game->how_many_occupied++;
            fds[seat_to_int(mess.iam.player)].fd = fds[i].fd;
            return 0;
        }
        else
            game->send_busy(fds[i].fd);
    }
    cout<<"Client not joined to the game.\n";
    close(fds[i].fd);
    fds[i].fd = -1;
    return 1;
}

void main_server_loop(pollfd *fds, Game_scenario game_scenario, int timeout) {
    timeval to = {.tv_sec = timeout, .tv_usec = 0};
    shared_ptr<Game_stage_server> game = make_shared<Game_stage_server>();
    while (true) {
        for (int i = 0; i < MAX_CLIENTS; i++) fds[i].revents = 0;
        cout<<"czekam\n";
        int poll_status = poll(fds, MAX_CLIENTS, -1);
        cout<<"wchodze\n";
        if (poll_status < 0) syserr("poll");
        if (poll_status == 0) continue;

        if (fds[4].revents & (POLLIN | POLLERR)) { // New client.
            sockaddr_in client_address;
            socklen_t client_address_len = sizeof(client_address);
            int client_fd = accept(fds[4].fd, (sockaddr*)&client_address, &client_address_len);
            if (client_fd < 0) syserr("accept");
            
            for (int i = 5; i < MAX_CLIENTS; i++) { // Add new not playing client.
                if (fds[i].fd == -1) {
                    fds[i].fd = client_fd;
                    fds[i].events = POLLIN;
                    // Set timeout for the client.
                    if (setsockopt(fds[i].fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0)
                        syserr("setsockopt");
                    if (fcntl(fds[i].fd, F_SETFL, O_NONBLOCK) < 0) syserr("fcntl"); // TODO: czy to potrzebne?
                    break;
                }
            }
            cout << "New client connected.\n";
        }
        cout<<fds[5].revents<<"\n";
        for (int i = 5; i < MAX_CLIENTS; i++) { // Not playing clients.
            if (fds[i].revents & (POLLIN | POLLERR)) { 
                // Try to join client to the game.
                if (join_client_to_game(fds, game, i)) continue;
                cout<< "Client joined to the game.\n";
                cout << "How many occupied: " << game->how_many_occupied << "\n";
                // If all players are joined to the game, start the game.
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

    // Sockets 0-3 are for playing clients in clockwise order. 
    // Socket 4 is for accepting new clients. 
    // All other sockets (4-(MAX_CLIENTS-1)) are for not playing (yet) clients.
    pollfd fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }
    fds[4].fd = socket_fd;

    main_server_loop(fds, game_scenario, timeout);
}