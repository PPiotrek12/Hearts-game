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

struct Game {
    bool first_message = true;
    bool receive_previous_taken = false;
    bool in_deal = false, in_trick = false;
    bool game_over = false;
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
    void print_all_tricks() {
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

Trick play_a_card(shared_ptr<Game> game, Trick trick);

#endif