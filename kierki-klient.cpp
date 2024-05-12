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


#include "common.h"
#include "messages.h"
#include "err.h"
#include <signal.h>

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

struct Game {
    bool first_message = true;
    bool receive_previous_taken = false;
    bool in_deal = false, in_trick = false;
    bool is_auto_playes = false, waiting_for_user_move = false;
    int act_trick_number = 0;
    int socket_fd;
    Deal act_deal;
    vector <Taken> all_taken;

    void add_taken(Taken taken) {
        all_taken.push_back(taken);
    }
    void clear_taken() { // TODO - potrzebne?
        all_taken.clear();
    }
    void remove_card(vector <Card> cards) {
        for (int i = 0; i < (int)(act_deal.cards).size(); i++) {
            for (int j = 0; j < (int)cards.size(); j++) {
                if (act_deal.cards[i].color == cards[j].color && 
                        act_deal.cards[i].value == cards[j].value) {
                    act_deal.cards.erase(act_deal.cards.begin() + i);
                    break;
                }
            }
        }
    }
    void print_all_taken() {
        for (int i = 0; i < (int)all_taken.size(); i++) {
            for (int j = 0; j < (int)all_taken[i].cards.size(); j++) {
                cout << all_taken[i].cards[j].to_str();
                if (j + 1 < (int)all_taken[i].cards.size()) cout << ", ";
            }
            cout << "\n";
        }
    }
    void print_avaible_cards() {
        for (int i = 0; i < (int)act_deal.cards.size(); i++) {
            cout << act_deal.cards[i].to_str();
            if (i + 1 < (int)act_deal.cards.size()) cout << ", ";
        }
        cout << "\n";
    }
};

Trick play_a_card(shared_ptr<Game> game, Trick trick) { // TODO: implement it better.
    if (game->is_auto_playes) {
        Trick res;
        res.trick_number = trick.trick_number;
        res.cards.push_back(game->act_deal.cards.back());
        return res;
    }
    cout << "Choose a card to play: ";
    string input;
    while (true) {
        cin >> input; // TODO: co jesli w tym miejscu uzytkownik zapyta o cards albo tricks?
        if (input.size() < 3 || input[0] != '!' || 
                !Card::is_card_correct(input.substr(1, input.size() - 1))) {
            cout << "Wrong card format, try again: ";
            continue;
        }
        else break;
    }
    Trick res;
    res.trick_number = trick.trick_number;
    res.cards.push_back(Card(input.substr(1, input.size() - 1)));
    return res;
}

void process_busy_message(shared_ptr<Game> game, Busy busy) {
    if (!game->first_message) return;
    if (!game->is_auto_playes) cout << busy.describe();
    exit(1);
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
    game->act_trick_number = trick.trick_number;
    game->receive_previous_taken = false;

    message move = {.trick = play_a_card(game, trick), .is_trick = true};
    send_message(game->socket_fd, move);
    game->remove_card(move.trick.cards);
}

void process_wrong_message(shared_ptr<Game> game, Wrong wrong) {
    if (!game->in_deal || game->in_trick) return;
    if (!game->is_auto_playes) cout << wrong.describe();
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
    if (connect(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address))<0)
        syserr("connect");

    pollfd fds[2];
    fds[0].fd = socket_fd; // The first socket is the server socket.
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO; // The second socket is the standard input.
    fds[1].events = POLLIN;

    message iam = {.iam = {.player = seat}, .is_iam = true};
    send_message(socket_fd, iam);

    shared_ptr<Game> game = make_shared<Game>();
    game->is_auto_playes = isAuto;
    game->socket_fd = socket_fd;

    while (true) {
        fds[0].revents = fds[1].revents = 0;
        int poll_status = poll(fds, 2, -1);
        if (poll_status < 0) syserr("poll");
        if (poll_status == 0) continue;
        if (fds[0].revents & POLLIN) {
            message mess = read_message(fds[0].fd);
            cout<<mess.to_message()<<endl;
            if (mess.is_busy) process_busy_message(game, mess.busy);
            if (mess.is_deal) process_deal_message(game, mess.deal);
            if (mess.is_trick) process_trick_message(game, mess.trick);
            if (mess.is_wrong) process_wrong_message(game, mess.wrong);
            if (mess.is_taken) process_taken_message(game, mess.taken);
            if (mess.is_score) process_score_message(game, mess.score);
            if (mess.is_total) process_total_message(game, mess.total);
        }
        if (fds[1].revents & POLLIN) {
            string input;
            getline(cin, input);
            if (input.empty()) continue;
            if (input == "cards") game->print_avaible_cards();
            else if (input == "tricks") game->print_all_taken();
            else cout << "Wrong command.\n";
        }
    }
    return 0;
}