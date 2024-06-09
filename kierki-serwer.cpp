#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "err.h"
#include "game_server.h"
#include "messages.h"

using namespace std;

const int QUEUE_LENGTH = 200;

void wrong_msg(int fd) {
    err("wrong message from client - closing connection");
    close(fd);
}

void parse_arguments(int argc, char *argv[], uint16_t *port, bool *wasPortSet, string *file,
                     int *timeout) {
    bool wasFileSet = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-p") {
            if (i + 1 >= argc)
                fatal("missing argument for -p");
            else {
                *port = read_port(argv[i + 1]);
                *wasPortSet = true;
                i++;
            }
        } else if (arg == "-f") {
            if (i + 1 >= argc)
                fatal("missing argument for -f");
            else {
                *file = argv[i + 1];
                wasFileSet = true;
                i++;
            }
        } else if (arg == "-t") {
            if (i + 1 >= argc)
                fatal("missing argument for -t");
            else {
                string timeout_str = argv[i + 1];
                for (char c : timeout_str)
                    if (!isdigit(c)) fatal("timeout must be a number");
                if ((int)timeout_str.size() > 7 || 
                        ((int)timeout_str.size() == 7 && timeout[0] != '1')) 
                    fatal("timeout too big");   
                *timeout = stoi(timeout_str);
                i++;
            }
        } else
            fatal("unknown argument");
    }
    if (!wasFileSet) fatal("missing -f argument");
}

// Accept new client and add it to the list of clients.
void accept_new_client(shared_ptr<Listener> listener, int timeout) {
    sockaddr_in6 client_address;
    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(listener->accepts.fd, (sockaddr *)&client_address, &client_address_len);
    if (client_fd < 0) syserr("accept");
    char address[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(client_address.sin6_addr), address, INET6_ADDRSTRLEN);
    string address_port = address;
    address_port += ":";
    address_port += to_string(ntohs(client_address.sin6_port));

    // Add new client to the list with default timeout.
    Timeout_fd new_client = {client_fd, timeout, 0, "", address_port};
    listener->clients.push_back(new_client);
}

// Closes all connections.
void end_game(shared_ptr<Listener> listener) {
    for (int i = 0; i < 4; i++) {
        if (listener->clients[i].fd != -1) close(listener->clients[i].fd);
    }
    close(listener->accepts.fd);
    for (int i = 4; i < (int)listener->clients.size(); i++) {
        if (listener->clients[i].fd != -1) close(listener->clients[i].fd);
    }
    exit(0);
}

void new_trick(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game, int first) {
    game->act_trick.trick_number++;
    game->act_trick.cards.clear();
    game->act_trick.how_many_played = 0;
    game->act_trick.act_player = first;
    game->act_trick.send_trick(listener->clients[first].fd, listener->clients[first].addr);
    listener->clients[first].timeout = game->timeout;
}

void new_deal(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    game->deal_number++;
    // If all deals are played then end the game.
    if (game->deal_number >= (int)game->game_scenario.deals.size()) end_game(listener);
    game->all_taken.clear();
    game->act_deal = game->game_scenario.deals[game->deal_number];
    game->act_deal.send_deals(listener);

    // Starting the first trick.
    game->act_trick.trick_number = 0;
    new_trick(listener, game, game->act_deal.first_player);
}

void rejoined(int fd, string addr, int seat, shared_ptr<Game_stage_server> game) {
    if (game->how_many_occupied == 4) game->game_stopped = false;
    message mess = {.deal = game->game_scenario.deals[game->deal_number].deals[seat],
                    .is_deal = true};
    send_message(fd, mess, addr, true);
    game->send_all_taken(fd, addr);
}

void proceed_message_from_not_playing(message mess, shared_ptr<Listener> listener,
                                      shared_ptr<Game_stage_server> game, int i) {
    int seat = seat_to_int(mess.iam.player);
    if (!mess.is_iam || seat == -1)  // Wrong message.
        wrong_msg(listener->clients[i].fd);
    else {  // Received IAM message, new we need to check if the seat is free.
        if (game->occupied[seat]) {
            game->send_busy(listener->clients[i].fd, listener->clients[i].addr);
            close(listener->clients[i].fd);
        } else {  // Seat is free.
            game->occupied[seat] = true;
            game->how_many_occupied++;
            listener->clients[seat] = listener->clients[i];
            listener->clients[seat].timeout = -1;
            listener->clients[seat].revents = 0;
            if (game->game_started)  // Resuming the game.
                rejoined(listener->clients[seat].fd, listener->clients[seat].addr, seat, game);
            else {
                if (game->how_many_occupied == 4) {
                    // Starting the game.
                    game->game_started = true;
                    new_deal(listener, game);
                }
            }
        }
    }
}

// Check activities on not playing clients.
void receive_from_not_playing(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    for (int i = 4; i < (int)listener->clients.size(); i++) {
        if (listener->clients[i].timeout <= 0) {
            close(listener->clients[i].fd);
            listener->clients.erase(listener->clients.begin() + i);
            i--;
        }
        if (listener->clients[i].revents & (POLLIN | POLLERR)) {
            int length =
                read_message(listener->clients[i].fd, &(listener->clients[i].buffer), true);
            if (!length)  // Client disconnected.
                close(listener->clients[i].fd);
            else {
                message mess =
                    parse_message(listener->clients[i].fd, &(listener->clients[i].buffer),
                                  listener->clients[i].addr);
                if (mess.empty) continue;
                proceed_message_from_not_playing(mess, listener, game, i);
            }
            listener->clients.erase(listener->clients.begin() + i);
            i--;
        }
    }
}

void ask_next_player(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    if (game->act_trick.how_many_played < 4) {
        game->act_trick.act_player = (game->act_trick.act_player + 1) % 4;
        listener->clients[game->act_trick.act_player].timeout = game->timeout;
        game->act_trick.send_trick(listener->clients[game->act_trick.act_player].fd,
                                   listener->clients[game->act_trick.act_player].addr);
    } else {  // End of the trick.
        int looser = find_looser_and_update_scores(game);
        Taken taken = {.trick_number = game->act_trick.trick_number,
                       .player = int_to_seat(looser),
                       .cards = game->act_trick.cards};
        game->all_taken.push_back(taken);
        message mess = {.taken = taken, .is_taken = true};
        for (int i = 0; i < 4; i++)
            send_message(listener->clients[i].fd, mess, listener->clients[i].addr, true);

        if (game->act_deal.deals[0].cards.empty()) {  // End of the deal.
            // Sending SCORE and TOTAL messages and starting new deal.
            message score = {.score = game->act_deal.scores, .is_score = true};
            message total = {.total = game->total_scores, .is_total = true};
            for (int i = 0; i < 4; i++) {
                send_message(listener->clients[i].fd, score, listener->clients[i].addr, true);
                send_message(listener->clients[i].fd, total, listener->clients[i].addr, true);
            }
            new_deal(listener, game);
        } else
            new_trick(listener, game, looser);
    }
}

void proceed_message_from_playing(message mess, shared_ptr<Listener> listener,
                                  shared_ptr<Game_stage_server> game, int i) {
    // Wrong message - closing connection and stopping the game.
    if (!mess.is_trick || (mess.is_trick && mess.trick.trick_number == -1)) {
        wrong_msg(listener->clients[i].fd);
        listener->clients[i] = {-1, -1, 0, "", ""};
        game->occupied[i] = false;
        game->how_many_occupied--;
        game->game_stopped = true;
        return;
    }
    // Received TRICK message.
    if (i != game->act_trick.act_player)  // Wrong player.
        game->act_trick.send_wrong(listener->clients[i].fd, listener->clients[i].addr);
    else {  // Correct player sent TRICK.
        bool correct = is_trick_correct(game, mess.trick, i);
        if (!correct) {
            // If player has chosen wrong card then send WRONG message.
            // We don't reset timeout here, because we ignore this
            // wrong message and wait for the correct one.
            game->act_trick.send_wrong(listener->clients[i].fd, listener->clients[i].addr);
        } else {
            listener->clients[i].timeout = -1;
            ask_next_player(listener, game);
        }
    }
}

void receive_from_playing(shared_ptr<Listener> listener, shared_ptr<Game_stage_server> game) {
    for (int i = 0; i < 4; i++) {
        if (listener->clients[i].fd == -1) continue;
        // If timeout passed then resend TRICK message.
        if (i == game->act_trick.act_player && listener->clients[i].timeout <= 0) {
            game->act_trick.send_trick(listener->clients[i].fd, listener->clients[i].addr);
            listener->clients[i].timeout = game->timeout;  // Reset timeout.
            continue;
        }
        if (listener->clients[i].revents & (POLLIN | POLLERR)) {
            fflush(stdout);
            int length =
                read_message(listener->clients[i].fd, &(listener->clients[i].buffer), true);
            if (!length) {  // Client disconnected.
                close(listener->clients[i].fd);
                listener->clients[i] = {-1, -1, 0, "", ""};
                game->occupied[i] = false;
                game->how_many_occupied--;
                if (game->game_started) game->game_stopped = true;
            }
        }
    }
    if (!game->game_stopped) {
        for (int i = 0; i < 4; i++) {
            message mess = parse_message(listener->clients[i].fd, &(listener->clients[i].buffer),
                                         listener->clients[i].addr);
            while (!mess.empty) {
                proceed_message_from_playing(mess, listener, game, i);
                mess = parse_message(listener->clients[i].fd, &(listener->clients[i].buffer),
                                     listener->clients[i].addr);
            }
        }
    }
}

// Main server loop. It listens on the socket_fd for new connections and on clients for messages.
void main_server_loop(int socket_fd, Game_scenario game_scenario, int timeout) {
    shared_ptr<Game_stage_server> game = make_shared<Game_stage_server>();
    game->game_scenario = game_scenario;
    game->timeout = timeout * 1000;

    // Create and fill Listener structure.
    shared_ptr<Listener> listener = make_shared<Listener>();
    listener->accepts = {socket_fd, -1, 0, "", ""};
    for (int i = 0; i < 4; i++) listener->clients.push_back({-1, -1, 0, "", ""});

    while (true) {
        listener->wrapped_poll(game->game_stopped);
        if (listener->accepts.revents & (POLLIN | POLLERR))  // New client is connecting.
            accept_new_client(listener, game->timeout);

        receive_from_not_playing(listener, game);
        if (!game->game_stopped) receive_from_playing(listener, game);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    uint16_t port;
    string file;
    int timeout = 5;
    bool wasPortSet = false;
    parse_arguments(argc, argv, &port, &wasPortSet, &file, &timeout);
    Game_scenario game_scenario;
    game_scenario.parse(file);  // Read game scenario from file.

    sockaddr_in6 server_address;
    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any;
    if (wasPortSet)
        server_address.sin6_port = htons(port);
    else
        server_address.sin6_port = 0;

    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) syserr("socket");

    int tmp = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0)
        syserr("setsockopt");

    if (bind(socket_fd, (sockaddr *)&server_address, sizeof(server_address)) < 0) syserr("bind");
    if (listen(socket_fd, QUEUE_LENGTH) < 0) syserr("listen");

    main_server_loop(socket_fd, game_scenario, timeout);
    return 0;
}