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

const int QUEUE_LENGTH = 50;

void wrong_msg(int fd) {
    err("wrong message from client - closing connection");
    close(fd);
}

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

// Accept new client and add it to the list of clients.
void accept_new_client(shared_ptr<Listener> listener, int timeout) {
    sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(listener->accepts.fd, (sockaddr*)&client_address, 
                                                            &client_address_len);
    if (client_fd < 0) syserr("accept");
    // Add new client to the list with default timeout.
    Timeout_fd new_client = {client_fd, timeout * 1000, 0, ""};
    listener->clients.push_back(new_client);
}

void end_game(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    // Send TOTAL message.
    vector <int> scores;
    vector <char> players;
    for (int i = 0; i < 4; i++) {
        scores.push_back(game->total_scores[i]);
        players.push_back(int_to_seat(i));
    }
    Score total = {.scores = scores, .players = players, .is_total = true};
    message msg = {.total = total, .is_total = true};
    for (int i = 0; i < 4; i++) {
        send_message(listener->clients[i].fd, msg);
        close(listener->clients[i].fd);
    }
    exit(0);
}

void rejoined(int fd, int seat, shared_ptr<Game_stage_server> game) {
    game->game_stopped = false;
    message mess = {.deal = game->act_deal.deals[seat], .is_deal = true};
    send_message(fd, mess);
    game->send_all_taken(fd);
}

void new_trick(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    game->act_trick.trick_number++;
    game->act_trick.cards.clear();
    game->act_trick.how_many_played = 0;
    for (int i = 0; i < 4; i++) 
        game->act_trick.played[i] = false;

    game->act_trick.act_player = game->act_deal.first_player;
    Trick trick = {.trick_number = game->act_trick.trick_number, .cards = {}};
    message mess = {.trick = trick, .is_trick = true};
    send_message(listener->clients[game->act_trick.act_player].fd, mess);
}

void new_deal(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    game->deal_number++;
    if (game->deal_number >= (int)game->game_scenario.deals.size()) end_game(listener, game);
    game->act_deal = game->game_scenario.deals[game->deal_number];
    for (int i = 0; i < 4; i++) {
        message mess = {.deal = game->act_deal.deals[i], .is_deal = true};
        send_message(listener->clients[i].fd, mess);
    }
    game->all_taken.clear();

    game->act_trick.trick_number = 0;
    new_trick(listener, game);
}

// Check activities on not playing clients.
void receive_from_not_playing(shared_ptr <Listener> listener, 
                              shared_ptr <Game_stage_server> game) {
    for (int i = 4; i < (int)listener->clients.size(); i++) {
        if (listener->clients[i].timeout <= 0) {
            close(listener->clients[i].fd);
            listener->clients.erase(listener->clients.begin() + i);
            i--;
        }
        if (listener->clients[i].revents & (POLLIN | POLLERR)) {
            message mess = read_message(listener->clients[i].fd, &(listener->clients[i].buffer));
            if (!mess.is_iam) {
                wrong_msg(listener->clients[i].fd);
                listener->clients.erase(listener->clients.begin() + i);
                i--;
                continue;
            }
            // Received IAM message, new we need to check if the seat is free.
            int seat = seat_to_int(mess.iam.player);
            if (game->occupied[seat]) {
                game->send_busy(listener->clients[i].fd);
                close(listener->clients[i].fd);
            }
            else {
                game->occupied[seat] = true;
                game->how_many_occupied++;
                listener->clients[seat] = listener->clients[i];
                listener->clients[seat].timeout = -1;
                if (game->how_many_occupied == 4) {
                    if (game->game_started) {
                        // Resuming the game.
                        rejoined(listener->clients[seat].fd, seat, game);
                    }
                    else {
                        // Starting the game.
                        game->game_started = true;
                        new_deal(listener, game);
                    }
                } 
            }
            listener->clients.erase(listener->clients.begin() + i);
            i--;
        }
    }
}

// Main server loop. It listens on the socket_fd for new connections and on clients for messages.
void main_server_loop(int socket_fd, Game_scenario game_scenario, int timeout) {
    shared_ptr<Game_stage_server> game = make_shared<Game_stage_server>();
    game->game_scenario = game_scenario;

    // Create and fill Listener structure.
    shared_ptr<Listener> listener = make_shared<Listener>();
    listener->accepts = {socket_fd, -1, 0, ""};
    for (int i = 0; i < 4; i++)
        listener->clients.push_back({-1, -1, 0, ""});

    while (true) {
        cout << "clients.size() = " << listener->clients.size() << "\n";
        listener->wrapped_poll();

        if (listener->accepts.revents & (POLLIN | POLLERR)) // New client is connecting.
            accept_new_client(listener, timeout);

        receive_from_not_playing(listener, game);
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
    game_scenario.parse(file); // Read game scenario from file.

    sockaddr_in6 server_address{};
    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any;
    if (wasPortSet) server_address.sin6_port = htons(port);

    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) syserr("socket");

    if (bind(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) < 0) syserr("bind"); // TODO: jak zrobic, zeby sluchac na wszystkich portach?
    if (listen(socket_fd, QUEUE_LENGTH) < 0) syserr("listen");

    main_server_loop(socket_fd, game_scenario, timeout);
}