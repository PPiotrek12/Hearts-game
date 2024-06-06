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

#define wrong_msg                                  \
    do {                                            \
        err("wrong message from server - ignoring");\
        return;                                     \
    } while(0)

using namespace std;

void parse_arguments(int argc, char* argv[], const char **host, uint16_t *port, bool *useIPv4,
                     bool *useIPv6, bool *is_auto_player, char *seat) {

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
        // Accept only first -4 or -6.
        else if (arg == "-4" && !(*useIPv4) && !(*useIPv6)) *useIPv4 = true;
        else if (arg == "-6" && !(*useIPv4) && !(*useIPv6)) *useIPv6 = true;
        else if (arg == "-N" || arg == "-E" || arg == "-S" || arg == "-W") {
            wasSeatSet = true;
            *seat = arg[1];
        }
        else if (arg == "-a") *is_auto_player = true;
        else fatal("unknown argument");
    }
    if (!wasHostSet) fatal("missing -h argument");
    if (!wasPortSet) fatal("missing -p argument");
    if (!wasSeatSet) fatal("missing seat argument");    
}

void process_busy_message(shared_ptr<Game_stage_client> game, Busy busy) {
    if (!game->first_message) wrong_msg;
    if (!game->is_auto_player) cout << busy.describe();
    game->first_message = false;
}

void process_deal_message(shared_ptr<Game_stage_client> game, Deal deal) {
    if (game->in_deal) wrong_msg;
    
    if (game->first_message) game->receive_previous_taken = true;
    else game->receive_previous_taken = false;
    game->first_message = false;

    if (!game->is_auto_player) cout << deal.describe();
    game->was_total = false;
    game->was_score = false;
    game->in_deal = true;
    game->act_deal = deal;
    game->act_trick_number = 0;
}

void process_trick_message(shared_ptr<Game_stage_client> game, Trick trick) {
    if (!game->in_deal) wrong_msg;
    if (!game->in_trick && trick.trick_number != game->act_trick_number + 1) wrong_msg;
    if (game->in_trick && trick.trick_number != game->act_trick_number) wrong_msg; // Retransmit.

    if (!game->is_auto_player) cout << trick.describe(game->act_deal.cards);
    game->in_trick = true;
    game->act_trick = trick;
    game->act_trick_number = trick.trick_number;
    game->receive_previous_taken = false;
    if (!game->is_auto_player) { // User player.
        game->waiting_for_card = true;
        game->ask_for_a_card();
    }
    else { // Auto player.
        Trick response = play_a_card(game, trick);
        message move = {.trick = response, .is_trick = true};
        send_message(game->socket_fd, move, game->is_auto_player);
    }
}

void process_wrong_message(shared_ptr<Game_stage_client> game, Wrong wrong) {
    if (!game->in_deal || !game->in_trick) wrong_msg;
    if (wrong.trick_number != game->act_trick_number) wrong_msg;
    if (game->waiting_for_card) wrong_msg;

    if (!game->is_auto_player) {
        cout << wrong.describe();
        game->waiting_for_card = true;
        game->ask_for_a_card();
        fflush(stdout);
    }
}

void process_taken_message(shared_ptr<Game_stage_client> game, Taken taken) {
    if (!game->in_deal) wrong_msg;
    if (!game->in_trick && !game->receive_previous_taken) wrong_msg;
    // Check if taken is from the current trick.
    if (game->in_trick && taken.trick_number != game->act_trick_number) wrong_msg;
    // Check if taken is after last taken (if not in trick).
    if (!game->in_trick && taken.trick_number != game->act_trick_number + 1) wrong_msg;
    if(game->waiting_for_card) wrong_msg;

    if (!game->is_auto_player) cout << taken.describe();
    game->remove_card(taken.cards);
    game->act_trick_number = taken.trick_number;
    game->in_trick = false;
    game->all_taken.push_back(taken);
}

void process_score_message(shared_ptr<Game_stage_client> game, Score score) {
    if (game->in_trick) wrong_msg;

    if (!game->is_auto_player) cout << score.describe();
    game->all_taken.clear();
    game->in_deal = false;
    game->was_score = true;
}

void process_total_message(shared_ptr<Game_stage_client> game, Score total) {
    if (game->in_trick) wrong_msg;

    if (!game->is_auto_player) cout << total.describe();
    game->all_taken.clear();
    game->in_deal = false;
    game->was_total = true;
}

void receive_server_message(shared_ptr<Game_stage_client> game, pollfd *fds) {
    int length = read_message(fds[0].fd, &(game->buffer_from_server), game->is_auto_player);
    if (!length) { // Server disconnected.
        close(fds[0].fd);
        if (game->was_total && game->was_score) exit(0);
        else exit(1);
    }
    message mess = parse_message(&(game->buffer_from_server));
    while (!mess.empty) {
        if (mess.is_busy) process_busy_message(game, mess.busy);
        if (mess.is_deal) process_deal_message(game, mess.deal);
        if (mess.is_trick) process_trick_message(game, mess.trick);
        if (mess.is_wrong) process_wrong_message(game, mess.wrong);
        if (mess.is_taken) process_taken_message(game, mess.taken);
        if (mess.is_score) process_score_message(game, mess.score);
        if (mess.is_total) process_total_message(game, mess.total);
        mess = parse_message(&(game->buffer_from_server));
    }
}

void receive_user_message(shared_ptr<Game_stage_client> game) {
    string input;
    getline(cin, input);
    if (input.empty()) return; // Ignore empty lines.
    if (input == "cards") game->print_avaible_cards(); // Print avaible cards.
    else if (input == "tricks") game->print_all_tricks(); // Print all tricks.
    else if (game->waiting_for_card && input[0] == '!' && 
            Card::is_card_correct(input.substr(1, input.size() - 1))) { // Play a card.
        vector <Card> choice = {Card(input.substr(1, input.size() - 1))};
        Trick response = Trick{.trick_number = game->act_trick_number, .cards = choice};
        message move = {.trick = response, .is_trick = true};
        send_message(game->socket_fd, move, game->is_auto_player);
        game->waiting_for_card = false;
    }
    else {
        cout << "Wrong command.\n";
        if (game->waiting_for_card) game->ask_for_a_card();
        fflush(stdout);
    }
}

void main_client_loop(pollfd *fds, bool is_auto, char seat) {
    shared_ptr<Game_stage_client> game = make_shared<Game_stage_client>();
    game->is_auto_player = is_auto;
    game->socket_fd = fds[0].fd;

    message iam = {.iam = {.player = seat}, .is_iam = true};
    send_message(fds[0].fd, iam, game->is_auto_player);
    int fds_nr = 2;
    if (is_auto) fds_nr = 1;

    while (true) {
        fds[0].revents = fds[1].revents = 0;
        int poll_status = poll(fds, fds_nr, -1);
        if (poll_status < 0) syserr("poll");
        if (poll_status == 0) continue;
        if (fds[0].revents & (POLLIN | POLLERR)) // Server message.
            receive_server_message(game, fds);
        if (!is_auto && (fds[1].revents & (POLLIN | POLLERR))) // User input.
            receive_user_message(game);
    }
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    const char *host;
    uint16_t port;
    bool useIPv4 = false, useIPv6 = false, is_auto = false;
    char seat;
    parse_arguments(argc, argv, &host, &port, &useIPv4, &useIPv6, &is_auto, &seat);

    sockaddr_in address4;
    sockaddr_in6 address6;
    int res = get_server_address(host, port, useIPv4, useIPv6, &address4, &address6);
    int socket_fd = 0;
    if (res == AF_INET6) {
        socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
        if (socket_fd == -1) syserr("socket");
        if (connect(socket_fd, (struct sockaddr *)&address6, (socklen_t)sizeof(address6)) < 0)
            syserr("connect");
    }
    else {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd == -1) syserr("socket");
        if (connect(socket_fd, (struct sockaddr *)&address4, (socklen_t)sizeof(address4)) < 0)
            syserr("connect");
    }

    pollfd fds[2]; // The first socket is the server socket and the second is the standard input.
    fds[0].fd = socket_fd;
    if (fds[0].fd == -1) syserr("socket");
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    main_client_loop(fds, is_auto, seat);
    return 0;
}