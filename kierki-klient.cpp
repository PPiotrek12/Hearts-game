#include <iostream>
#include <string>
#include <netinet/in.h>

#include "common.h"
#include "messages.h"
#include "err.h"
#include <signal.h>

using namespace std;

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

int play_deal(int socket_fd, Deal deal, bool first, bool isAutoPlayer) {
    message mess;
    for (int i = 1; i <= 13; i++) {
        do {
            mess = read_message(socket_fd);
        } while (!mess.is_trick && !mess.is_score && !(first && mess.is_taken));

        if (mess.is_score) break; // The end of the deal.

        if (mess.is_taken) { // Tricks played before our joining.
            if (isAutoPlayer) cout << mess.taken.describe();
            remove_card(&(deal.cards), mess.taken);
            continue;
        }

        // Our turn in this trick.
        if (isAutoPlayer) {
            cout << mess.trick.describe(deal.cards);
            // TODO: ask user for a Card.
        }

        message move = {.trick = put_card(deal.cards, mess.trick), .is_trick = true};
        send_message(socket_fd, move);

        do {
            mess = read_message(socket_fd);
        } while (!mess.is_taken);
        if (isAutoPlayer) cout << mess.taken.describe();
        remove_card(&(deal.cards), mess.taken);
    }
    while (!mess.is_score)
        mess = read_message(socket_fd);
    if (!isAutoPlayer) cout << mess.score.describe();
    return 0;
}

int main(int argc, char* argv[]) {
    const char *host;
    uint16_t port;
    bool useIPv4 = false, useIPv6 = false, isAutoPlayer = false;
    char seat;
    parse_arguments(argc, argv, &host, &port, &useIPv4, &useIPv6, &isAutoPlayer, &seat);

    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in server_address = get_server_address(host, port, useIPv4, useIPv6);
    int socket_fd = socket(server_address.sin_family, SOCK_STREAM, 0);
    if (socket_fd == -1) syserr("socket");
    // if (connect(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address))<0)
    //     syserr("connect");
    socket_fd = 0; // TODO

    message iam = {.iam = {.player = seat}, .is_iam = true};
    send_message(socket_fd, iam);

    bool first_message = true;
    bool in_deal = false;
    bool in_trick = false;
    bool receive_previous_taken = false;
    Deal act_deal;
    int act_trick_number = 0;

    while (true) {
        message mess = read_message(socket_fd);

        if (mess.is_busy) {
            if (first_message) {
                if (!isAutoPlayer) cout << mess.busy.describe();
                return 1;
            }
        }
        if (mess.is_deal) {
            if (in_deal) continue;

            if (first_message) receive_previous_taken = true;
            else receive_previous_taken = false;

            if (!isAutoPlayer) cout << mess.deal.describe();
            in_deal = true;
            act_deal = mess.deal;
            first_message = false;
            act_trick_number = 0;
        }
        if (mess.is_trick) {
            if (!in_deal || in_trick) continue;
            if (mess.trick.trick_number != act_trick_number + 1) continue;

            if (!isAutoPlayer) cout << mess.trick.describe(act_deal.cards);
            in_trick = true;
            act_trick_number = mess.trick.trick_number;
            receive_previous_taken = false;
            message move = {.trick = put_card(act_deal.cards, mess.trick), .is_trick = true};
            send_message(socket_fd, move);
        }
        if (mess.is_taken) {
            if (!in_deal || (!in_trick && !receive_previous_taken)) continue;
            // Check if taken is from the current trick.
            if (in_trick && mess.taken.trick_number != act_trick_number) continue;
            // Check if taken is after last taken (if not in trick).
            if (!in_trick && mess.taken.trick_number != act_trick_number + 1) continue;

            if (!isAutoPlayer) cout << mess.taken.describe();
            act_trick_number = mess.taken.trick_number;
            in_trick = false;
            remove_card(&(act_deal.cards), mess.taken);
        }
        if (mess.is_score) {
            if (!in_deal || in_trick) continue;

            if (!isAutoPlayer) cout << mess.score.describe();
            in_deal = false;
        }
        if (mess.is_total) {
            if (in_deal) continue;

            if (!isAutoPlayer) cout << mess.total.describe();
            return 0;
        }
    }

   return 0;
}