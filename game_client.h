#ifndef MIM_GAME_CLIENT_H
#define MIM_GAME_CLIENT_H

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

using namespace std;

// Struct representing a game stage in a specific moment for a client.
struct Game_stage_client {
    bool first_message = true;
    bool receive_previous_taken = false;
    bool in_deal = false, in_trick = false;
    bool was_total = false;
    bool is_auto_player = false;
    bool waiting_for_card = false;
    int act_trick_number = 0;
    int socket_fd;
    Deal act_deal;
    Trick act_trick;
    vector <Taken> all_taken;
    string buffer_from_server;

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

    void ask_for_a_card() {
        cout << "Waiting for your card choice (format: '!' + card code); " <<
                "you still can type 'tricks'/'cards' command:\n";
    }

    void print_all_tricks() {
        for (int i = 0; i < (int)all_taken.size(); i++) {
            for (int j = 0; j < (int)all_taken[i].cards.size(); j++) {
                cout << all_taken[i].cards[j].to_str();
                if (j + 1 < (int)all_taken[i].cards.size()) cout << ", ";
            }
            cout << "\n";
        }
        if (waiting_for_card) ask_for_a_card();
    }

    void print_avaible_cards() {
        for (int i = 0; i < (int)act_deal.cards.size(); i++) {
            cout << act_deal.cards[i].to_str();
            if (i + 1 < (int)act_deal.cards.size()) cout << ", ";
        }
        cout << "\n";
        if (waiting_for_card) ask_for_a_card();
    }
};

Trick play_a_card(shared_ptr<Game_stage_client> game, Trick trick);

#endif