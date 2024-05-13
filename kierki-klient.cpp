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
#include "game_client.h"

using namespace std;

void parse_arguments(int argc, char* argv[], const char **host, uint16_t *port, bool *useIPv4,
                     bool *useIPv6, bool *is_auto_playes, char *seat) {

    bool wasHostSet = false, wasPortSet = false, wasSeatSet = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-h") {
            if (i + 1 >= argc) fatal("missing argument for -h");
            else {
                *host = argv[i + 1];
                wasHostSet = true;
                i++;
            }
        } 
        else if (arg == "-p") {
            if (i + 1 >= argc) fatal("missing argument for -p");
            else {
                *port = read_port(argv[i + 1]);
                wasPortSet = true;
                i++;
            }
        }
        else if (arg == "-4") *useIPv4 = true;
        else if (arg == "-6") *useIPv6 = true;
        else if (arg == "-N" || arg == "-E" || arg == "-S" || arg == "-W") {
            wasSeatSet = true;
            *seat = arg[1];
        }
        else if (arg == "-a") *is_auto_playes = true;
        else fatal("unknown argument");
    }
    if (!wasHostSet) fatal("missing -h argument");
    if (!wasPortSet) fatal("missing -p argument");
    if (!wasSeatSet) fatal("missing seat argument");    
}

void process_busy_message(shared_ptr<Game> game, Busy busy) {
    if (!game->first_message) return;
    if (!game->is_auto_playes) cout << busy.describe();
}

void process_deal_message(shared_ptr<Game> game, Deal deal) {
    if (game->in_deal) return;

    if (game->first_message) game->receive_previous_taken = true;
    else game->receive_previous_taken = false;

    if (!game->is_auto_playes) cout << deal.describe();
    game->in_deal = true;
    game->act_deal = deal;
    game->first_message = false;
    game->act_trick_number = 0;
}

void process_trick_message(shared_ptr<Game> game, Trick trick) {
    if (!game->in_deal || game->in_trick) return;
    if (trick.trick_number != game->act_trick_number + 1) return;

    if (!game->is_auto_playes) cout << trick.describe(game->act_deal.cards);
    game->in_trick = true;
    game->act_trick = trick;
    game->act_trick_number = trick.trick_number;
    game->receive_previous_taken = false;

    message move = {.trick = play_a_card(game, game->act_trick), .is_trick = true};
    send_message(game->socket_fd, move);
    game->remove_card(move.trick.cards);
}

void process_wrong_message(shared_ptr<Game> game, Wrong wrong) {
    if (!game->in_deal || !game->in_trick) return;
    if (wrong.trick_number != game->act_trick_number) return;
    if (!game->is_auto_playes) cout << wrong.describe();

    message move = {.trick = play_a_card(game, game->act_trick), .is_trick = true};
    send_message(game->socket_fd, move);
    game->remove_card(move.trick.cards);
}

void process_taken_message(shared_ptr<Game> game, Taken taken) {
    if (!game->in_deal || (!game->in_trick && !game->receive_previous_taken)) return;
    // Check if taken is from the current trick.
    if (game->in_trick && taken.trick_number != game->act_trick_number) return;
    // Check if taken is after last taken (if not in trick).
    if (!game->in_trick && taken.trick_number != game->act_trick_number + 1) return;

    if (!game->is_auto_playes) cout << taken.describe();
    game->act_trick_number = taken.trick_number;
    game->in_trick = false;
    game->add_taken(taken);
    if (!game->in_trick && game->receive_previous_taken) // Remove cards from previous trick.
        game->remove_card(taken.cards);
}

void process_score_message(shared_ptr<Game> game, Score score) {
    if (!game->in_deal || game->in_trick) return;

    if (!game->is_auto_playes) cout << score.describe();
    game->in_deal = false;
}

void process_total_message(shared_ptr<Game> game, Score total) {
    if (game->in_deal || game->in_trick) return;

    if (!game->is_auto_playes) cout << total.describe();
    game->game_over = true;
}

void main_client_loop(pollfd *fds, bool is_auto, char seat) {
    shared_ptr<Game> game = make_shared<Game>();
    game->is_auto_playes = is_auto;
    game->socket_fd = fds[0].fd;

    message iam = {.iam = {.player = seat}, .is_iam = true};
    send_message(fds[0].fd, iam);
    
    int fds_nr = 2;
    if (is_auto) fds_nr = 1;

    while (true) {
        fds[0].revents = fds[1].revents = 0;
        int poll_status = poll(fds, fds_nr, -1);
        if (poll_status < 0) syserr("poll");
        if (poll_status == 0) continue;
        if (fds[0].revents & POLLIN) { // Server message.
            message mess = read_message(fds[0].fd);
            if (mess.closed_connection) {
                if (game->game_over) exit(0);
                else exit(1);
            }
            if (mess.is_busy) process_busy_message(game, mess.busy);
            if (mess.is_deal) process_deal_message(game, mess.deal);
            if (mess.is_trick) process_trick_message(game, mess.trick);
            if (mess.is_wrong) process_wrong_message(game, mess.wrong);
            if (mess.is_taken) process_taken_message(game, mess.taken);
            if (mess.is_score) process_score_message(game, mess.score);
            if (mess.is_total) process_total_message(game, mess.total);
        }
        if (!is_auto && (fds[1].revents & POLLIN)) { // User input.
            string input;
            getline(cin, input);
            if (input.empty()) continue;
            if (input == "cards") game->print_avaible_cards();
            else if (input == "tricks") game->print_all_tricks();
            else cout << "Wrong command.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    const char *host;
    uint16_t port;
    bool useIPv4 = false, useIPv6 = false, is_auto = false;
    char seat;
    parse_arguments(argc, argv, &host, &port, &useIPv4, &useIPv6, &is_auto, &seat);

    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in server_address = get_server_address(host, port, useIPv4, useIPv6);

    pollfd fds[2]; // The first socket is the server socket and the second is the standard input.
    fds[0].fd = socket(server_address.sin_family, SOCK_STREAM, 0); 
    if (fds[0].fd == -1) syserr("socket");
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    if (connect(fds[0].fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address))<0)
        syserr("connect");

    main_client_loop(fds, is_auto, seat);
    return 0;
}