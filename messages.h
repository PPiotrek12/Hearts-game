#ifndef MIM_MESSAGES_H
#define MIM_MESSAGES_H

#include <string>
#include <vector>

using namespace std;

struct card {
    string color;
    int value;
    card(string mess) {
        if (mess[0] == '1') value = 10;
        else if (mess[0] == 'J') value = 11;
        else if (mess[0] == 'Q') value = 12;
        else if (mess[0] == 'K') value = 13;
        else if (mess[0] == 'A') value = 14;
        else value = mess[0] - '0';
        color = mess[mess.size() - 1];
    }
};

struct IAM_message {
    char player;
    void parse(string mess) {
        player = mess[0];
    }
};

struct BUSY_message {
    vector <char> players;
    void parse(string mess) {
        for (int i = 0; i < (int)mess.size(); i++)
            players.push_back(mess[i]);
    }
};

struct DEAL_message {
    int deal_type;
    char first_player;
    vector <card> cards;
    void parse(string mess) {
        deal_type = mess[0] - '0';
        first_player = mess[1];
        for (int i = 2; i < (int)mess.size(); i++) {
            if (mess[i] == '1') {
                cards.push_back(card(mess.substr(i, 3)));
                i += 2;
            }
            else {
                cards.push_back(card(mess.substr(i, 2)));
                i++;
            }
        }
    }
};

struct TRICK_message {
    int trick_number;
    vector <card> cards;
    void parse(string mess) {
        trick_number = mess[0] - '0';
        int i = 1;
        if ((mess.size() > 3 && (mess[3] == 'S' ||
                mess[3] == 'H' || mess[3] == 'D' || mess[3] == 'C')) || mess.size() == 2){
            trick_number *= 10;
            trick_number += mess[1] - '0';
            i++;
        }
        for (; i < (int)mess.size(); i++) {
            if (mess[i] == '1') {
                cards.push_back(card(mess.substr(i, 3)));
                i += 2;
            }
            else {
                cards.push_back(card(mess.substr(i, 2)));
                i++;
            }
        }
    }
};

struct WRONG_message {
    int trick_number;
    void parse(string mess) {
        trick_number = mess[0] - '0';
        if (mess.size() > 1) {
            trick_number *= 10;
            trick_number += mess[1] - '0';
        }
    }
};

struct TAKEN_message {
    int trick_number;
    char player;
    vector <card> cards;
    void parse(string mess) {
        trick_number = mess[0] - '0';
        int i = 1;
        if ((mess.size() > 3 && (mess[3] == 'S' ||
                mess[3] == 'H' || mess[3] == 'D' || mess[3] == 'C')) || mess.size() == 2){
            trick_number *= 10;
            trick_number += mess[1] - '0';
            i++;
        }
        for (; i < (int)mess.size() - 1; i++) {
            if (mess[i] == '1') {
                cards.push_back(card(mess.substr(i, 3)));
                i += 2;
            }
            else {
                cards.push_back(card(mess.substr(i, 2)));
                i++;
            }
        }
        player = mess[mess.size() - 1];
    }
};

struct SCORE_message {
    vector <int> scores;
    vector <char> players;
    void parse(string mess) {
        int last_palyer = -1;
        for (int i = 0; i < (int)mess.size(); i++) {
            if (mess[i] == 'E' || mess[i] == 'N' || mess[i] == 'W' || mess[i] == 'S') {
                if (last_palyer != -1)
                    scores.push_back(atoi(mess.substr(last_palyer + 1, i - last_palyer - 1).c_str()));
                last_palyer = i;
                players.push_back(mess[i]);
            }
        }
        scores.push_back(atoi(mess.substr(last_palyer + 1, mess.size() - last_palyer - 1).c_str()));
    }
};

struct message {
    IAM_message iam;
    BUSY_message busy;
    DEAL_message deal;
    TRICK_message trick;
    WRONG_message wrong;
    TAKEN_message taken;
    SCORE_message score, total;
    bool is_im = false, is_busy = false, is_deal = false, is_trick = false;
    bool is_wrong = false, is_taken = false, is_score = false, is_total = false;
    void parse(string mess) {
        if (mess.substr(0, 3) == "IAM") {
            iam.parse(mess.substr(3, mess.size() - 3));
            is_im = true;
        } else if (mess.substr(0, 4) == "BUSY") {
            busy.parse(mess.substr(4, mess.size() - 4));
            is_busy = true;
        } else if (mess.substr(0, 4) == "DEAL") {
            deal.parse(mess.substr(4, mess.size() - 4));
            is_deal = true;
        } else if (mess.substr(0, 5) == "TRICK") {
            trick.parse(mess.substr(5, mess.size() - 5));
            is_trick = true;
        } else if (mess.substr(0, 5) == "WRONG") {
            wrong.parse(mess.substr(5, mess.size() - 5));
            is_wrong = true;
        } else if (mess.substr(0, 5) == "TAKEN") {
            taken.parse(mess.substr(5, mess.size() - 5));
            is_taken = true;
        } else if (mess.substr(0, 5) == "SCORE") {
            score.parse(mess.substr(5, mess.size() - 5));
            is_score = true;
        } else if (mess.substr(0, 5) == "TOTAL") {
            total.parse(mess.substr(5, mess.size() - 5));
            is_total = true;
        }
    }
};

message read_message(int fd);

#endif