#include <iostream>
#include <string>
#include <netinet/in.h>

#include "common.h"
#include "messages.h"
#include "err.h"
#include <signal.h>

using namespace std;

struct Game {
    bool first_message = true;
    bool in_deal = false;
    bool in_trick = false;
    bool receive_previous_taken = false;
    bool isAutoPlayer = false;
    Deal act_deal;
    int act_trick_number = 0;
    int socket_fd;
};

void parse_arguments(int argc, char* argv[], const char **host, uint16_t *port, bool *useIPv4,
                     bool *useIPv6, bool *isAutoPlayer, char *seat) {

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
        else if (arg == "-a") *isAutoPlayer = true;
        else fatal("unknown argument");
    }
    if (!wasHostSet) fatal("missing -h argument");
    if (!wasPortSet) fatal("missing -p argument");
    if (!wasSeatSet) fatal("missing seat argument");    
}

Trick put_card(vector <Card> avaible_cards, Trick trick) { // TODO: implement it better.
    Trick res;
    res.trick_number = trick.trick_number;
    res.cards.push_back(avaible_cards.back());
    return res;
}
void remove_card(vector <Card> *avaible_cards, Taken taken) {
    for (int i = 0; i < (int)(*avaible_cards).size(); i++) {
        for (int j = 0; j < (int)taken.cards.size(); j++) {
            if ((*avaible_cards)[i].color == taken.cards[j].color && 
                    (*avaible_cards)[i].value == taken.cards[j].value) {
                (*avaible_cards).erase((*avaible_cards).begin() + i);
                break;
            }
        }
    }
}

void process_busy_message(Game *game, Busy busy) {
    if (!game->first_message) return;
    if (!game->isAutoPlayer) cout << busy.describe();
    exit(1);
}

void process_deal_message(Game *game, Deal deal) {
    if (game->in_deal) return;

    if (game->first_message) game->receive_previous_taken = true;
    else game->receive_previous_taken = false;

    if (!game->isAutoPlayer) cout << deal.describe();
    game->in_deal = true;
    game->act_deal = deal;
    game->first_message = false;
    game->act_trick_number = 0;
}

void process_trick_message(Game *game, Trick trick) {
    if (!game->in_deal || game->in_trick) return;
    if (trick.trick_number != game->act_trick_number + 1) return;

    if (!game->isAutoPlayer) cout << trick.describe(game->act_deal.cards);
    game->in_trick = true;
    game->act_trick_number = trick.trick_number;
    game->receive_previous_taken = false;
    message move = {.trick = put_card(game->act_deal.cards, trick), .is_trick = true};
    send_message(game->socket_fd, move);
}

void process_taken_message(Game *game, Taken taken) {
    if (!game->in_deal || (!game->in_trick && !game->receive_previous_taken)) return;
    // Check if taken is from the current trick.
    if (game->in_trick && taken.trick_number != game->act_trick_number) return;
    // Check if taken is after last taken (if not in trick).
    if (!game->in_trick && taken.trick_number != game->act_trick_number + 1) return;

    if (!game->isAutoPlayer) cout << taken.describe();
    game->act_trick_number = taken.trick_number;
    game->in_trick = false;
    remove_card(&(game->act_deal.cards), taken);
}

void process_score_message(Game *game, Score score) {
    if (!game->in_deal || game->in_trick) return;

    if (!game->isAutoPlayer) cout << score.describe();
    game->in_deal = false;
}

void process_total_message(Game *game, Score total) {
    if (game->in_deal || game->in_trick) return;

    if (!game->isAutoPlayer) cout << total.describe();
    exit(0);
}

int main(int argc, char* argv[]) {
    const char *host;
    uint16_t port;
    bool useIPv4 = false, useIPv6 = false, isAuto = false;
    char seat;
    parse_arguments(argc, argv, &host, &port, &useIPv4, &useIPv6, &isAuto, &seat);

    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in server_address = get_server_address(host, port, useIPv4, useIPv6);
    int socket_fd = socket(server_address.sin_family, SOCK_STREAM, 0);
    if (socket_fd == -1) syserr("socket");
    // if (connect(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address))<0)
    //     syserr("connect");
    socket_fd = 0; // TODO

    message iam = {.iam = {.player = seat}, .is_iam = true};
    send_message(socket_fd, iam);

    Game *game = new Game();
    game->isAutoPlayer = isAuto;
    game->socket_fd = socket_fd;

    while (true) {
        message mess = read_message(socket_fd);
        if (mess.is_busy) process_busy_message(game, mess.busy);
        if (mess.is_deal) process_deal_message(game, mess.deal);
        if (mess.is_trick) process_trick_message(game, mess.trick);
        if (mess.is_taken) process_taken_message(game, mess.taken);
        if (mess.is_score) process_score_message(game, mess.score);
        if (mess.is_total) process_total_message(game, mess.total);
    }

   return 0;
}