#ifndef MIM_MESSAGES_H
#define MIM_MESSAGES_H

#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// Struct representing a card.
struct Card {
    char color;
    int value;
    Card(string mess) {
        if (mess[0] == '1' && mess[1] == '0')
            value = 10;
        else if (mess[0] == 'J')
            value = 11;
        else if (mess[0] == 'Q')
            value = 12;
        else if (mess[0] == 'K')
            value = 13;
        else if (mess[0] == 'A')
            value = 14;
        else
            value = mess[0] - '0';
        color = mess[mess.size() - 1];
    }
    static bool is_card_correct(string mess) {
        if (mess.size() == 2) {
            if ((mess[0] >= '2' && mess[0] <= '9') || mess[0] == 'J' || mess[0] == 'Q' ||
                mess[0] == 'K' || mess[0] == 'A')
                if (mess[1] == 'S' || mess[1] == 'H' || mess[1] == 'D' || mess[1] == 'C')
                    return true;
        } else if (mess.size() == 3) {
            if (mess[0] == '1' && mess[1] == '0' &&
                (mess[2] == 'S' || mess[2] == 'H' || mess[2] == 'D' || mess[2] == 'C'))
                return true;
        }
        return false;
    }
    string to_str() {
        string res;
        if (value == 10)
            res += "10";
        else if (value == 11)
            res += "J";
        else if (value == 12)
            res += "Q";
        else if (value == 13)
            res += "K";
        else if (value == 14)
            res += "A";
        else
            res += string(1, value + '0');
        res += string(1, color);
        return res;
    }
};

// Struct representing an IAM message.
struct Iam {
    char player;
    void parse(string mess) {
        if (mess.size() == 1)
            player = mess[0];
        else
            player = 'X';
    }
    string to_message() { return "IAM" + string(1, player) + "\r\n"; }
};

// Struct representing a BUSY message.
struct Busy {
    vector<char> players;
    void parse(string mess) {
        for (int i = 0; i < (int)mess.size(); i++) players.push_back(mess[i]);
    }
    string to_message() {
        string res = "BUSY";
        for (int i = 0; i < (int)players.size(); i++) res += string(1, players[i]);
        res += "\r\n";
        return res;
    }
    string describe() {
        string res = "Place busy, list of busy places received: ";
        for (int i = 0; i < (int)players.size(); i++) {
            res += string(1, players[i]);
            if (i != (int)players.size() - 1) res += ", ";
        }
        res += ".\n";
        return res;
    }
};

// Struct representing a deal for one player.
struct Deal {
    int deal_type;
    char first_player;
    vector<Card> cards;
    void parse(string mess) {
        deal_type = mess[0] - '0';
        first_player = mess[1];
        for (int i = 2; i < (int)mess.size(); i++) {
            if (mess[i] == '1') {
                cards.push_back(Card(mess.substr(i, 3)));
                i += 2;
            } else {
                cards.push_back(Card(mess.substr(i, 2)));
                i++;
            }
        }
    }
    string to_message() {
        string res = "DEAL" + string(1, deal_type + '0') + string(1, first_player);
        for (int i = 0; i < (int)cards.size(); i++) res += cards[i].to_str();
        res += "\r\n";
        return res;
    }
    string describe() {
        string res = "New deal " + to_string(deal_type) + ": staring place " +
                     string(1, first_player) + ", your cards: ";
        for (int i = 0; i < (int)cards.size(); i++) {
            res += cards[i].to_str();
            if (i != (int)cards.size() - 1) res += ", ";
        }
        res += ".\n";
        return res;
    }
};

// Struct representing a trick for one player. // If trick_number == -1,
// then message is completly wrong - disconnecting client.
struct Trick {
    int trick_number;
    vector<Card> cards;
    void parse(string mess) {
        trick_number = mess[0] - '0';
        mess = mess.substr(1, mess.size() - 1);
        // Check if card code doesn't start on the first index => trick number is two-digit.
        if ((int)mess.size() > 0 && !Card::is_card_correct(mess.substr(0, 2)) &&
            !Card::is_card_correct(mess.substr(0, 3))) {
            trick_number *= 10;
            trick_number += mess[0] - '0';
            mess = mess.substr(1, mess.size() - 1);
        }
        if (trick_number > 13 || trick_number < 1) trick_number = -1;
        if (mess.size() == 1 || (mess.size() > 1 && !Card::is_card_correct(mess.substr(0, 2)) &&
                                 !Card::is_card_correct(mess.substr(0, 3))))
            trick_number = -1;
        if (trick_number == -1) return;
        for (int i = 0; i < (int)mess.size(); i++) {
            if (mess[i] == '1') {
                cards.push_back(Card(mess.substr(i, 3)));
                i += 2;
            } else {
                cards.push_back(Card(mess.substr(i, 2)));
                i++;
            }
        }
    }
    string to_message() {
        string res = "TRICK";
        if (trick_number > 9) res += string(1, trick_number / 10 + '0');
        res += string(1, trick_number % 10 + '0');
        for (int i = 0; i < (int)cards.size(); i++) res += cards[i].to_str();
        res += "\r\n";
        return res;
    }
    string describe(vector<Card> avaible_cards) {
        string res = "Trick: (" + to_string(trick_number) + ") ";
        for (int i = 0; i < (int)cards.size(); i++) {
            res += cards[i].to_str();
            if (i != (int)cards.size() - 1) res += ", ";
        }
        res += "\n";
        res += "Available: ";
        for (int i = 0; i < (int)avaible_cards.size(); i++) {
            res += avaible_cards[i].to_str();
            if (i != (int)avaible_cards.size() - 1) res += ", ";
        }
        res += "\n";
        return res;
    }
};

// Struct representing a WRONG message.
struct Wrong {
    int trick_number;
    void parse(string mess) {
        trick_number = mess[0] - '0';
        if (mess.size() > 1) {
            trick_number *= 10;
            trick_number += mess[1] - '0';
        }
    }
    string to_message() {
        string res = "WRONG";
        if (trick_number > 9) res += string(1, trick_number / 10 + '0');
        res += string(1, trick_number % 10 + '0');
        res += "\r\n";
        return res;
    }
    string describe() {
        return "Wrong message received in trick " + to_string(trick_number) + ".\n";
    }
};

// Struct representing a TAKEN message.
struct Taken {
    int trick_number;
    char player;
    vector<Card> cards;
    void parse(string mess) {
        trick_number = mess[0] - '0';
        int i = 1;
        // Check if card code doesn't start on the first index => trick number is two-digit.
        if ((int)mess.size() > 1 && !Card::is_card_correct(mess.substr(1, 2)) &&
            !Card::is_card_correct(mess.substr(1, 3))) {
            trick_number *= 10;
            trick_number += mess[1] - '0';
            i++;
        }
        for (; i < (int)mess.size() - 1; i++) {
            if (mess[i] == '1') {
                cards.push_back(Card(mess.substr(i, 3)));
                i += 2;
            } else {
                cards.push_back(Card(mess.substr(i, 2)));
                i++;
            }
        }
        player = mess[mess.size() - 1];
    }
    string to_message() {
        string res = "TAKEN";
        if (trick_number > 9) res += string(1, trick_number / 10 + '0');
        res += string(1, trick_number % 10 + '0');
        for (int i = 0; i < (int)cards.size(); i++) res += cards[i].to_str();
        res += string(1, player);
        res += "\r\n";
        return res;
    }
    string describe() {
        string res = "A trick " + to_string(trick_number) + " is taken by " + player + ", cards ";
        for (int i = 0; i < (int)cards.size(); i++) {
            res += cards[i].to_str();
            if (i != (int)cards.size() - 1) res += ", ";
        }
        res += ".\n";
        return res;
    }
};

// Struct representing a SCORE and TAKEN message.
struct Score {
    vector<int> scores = {0, 0, 0, 0};
    vector<char> players = {'N', 'E', 'S', 'W'};
    bool is_total = false;
    void parse(string mess, bool total = false) {
        scores.clear();
        players.clear();
        is_total = total;
        int last_palyer = -1;
        for (int i = 0; i < (int)mess.size(); i++) {
            if (mess[i] == 'E' || mess[i] == 'N' || mess[i] == 'W' || mess[i] == 'S') {
                if (last_palyer != -1)
                    scores.push_back(
                        atoi(mess.substr(last_palyer + 1, i - last_palyer - 1).c_str()));
                last_palyer = i;
                players.push_back(mess[i]);
            }
        }
        scores.push_back(atoi(mess.substr(last_palyer + 1, mess.size() - last_palyer - 1).c_str()));
    }
    string to_message() {
        string res;
        if (is_total)
            res = "TOTAL";
        else
            res = "SCORE";
        for (int i = 0; i < (int)scores.size(); i++) {
            res += string(1, players[i]);
            res += to_string(scores[i]);
        }
        res += "\r\n";
        return res;
    }
    string describe() {
        string res;
        if (is_total)
            res = "Total scores are:\n";
        else
            res = "The scores are:\n";
        for (int i = 0; i < (int)scores.size(); i++)
            res += string(1, players[i]) + " | " + to_string(scores[i]) + "\n";
        res += "\n";
        return res;
    }
};

// Struct representing a message.
struct message {
    Iam iam = {};
    Busy busy = {};
    Deal deal = {};
    Trick trick = {};
    Wrong wrong = {};
    Taken taken = {};
    Score score = {}, total = {};
    bool closed_connection = false;
    bool empty = false;
    bool is_iam = false, is_busy = false, is_deal = false, is_trick = false;
    bool is_wrong = false, is_taken = false, is_score = false, is_total = false;
    void parse(string mess) {
        if (mess.size() >= 3 && mess.substr(0, 3) == "IAM") {
            iam.parse(mess.substr(3, mess.size() - 3));
            is_iam = true;
        } else if (mess.size() >= 4 && mess.substr(0, 4) == "BUSY") {
            busy.parse(mess.substr(4, mess.size() - 4));
            is_busy = true;
        } else if (mess.size() >= 4 && mess.substr(0, 4) == "DEAL") {
            deal.parse(mess.substr(4, mess.size() - 4));
            is_deal = true;
        } else if (mess.size() >= 5 && mess.substr(0, 5) == "TRICK") {
            trick.parse(mess.substr(5, mess.size() - 5));
            is_trick = true;
        } else if (mess.size() >= 5 && mess.substr(0, 5) == "WRONG") {
            wrong.parse(mess.substr(5, mess.size() - 5));
            is_wrong = true;
        } else if (mess.size() >= 5 && mess.substr(0, 5) == "TAKEN") {
            taken.parse(mess.substr(5, mess.size() - 5));
            is_taken = true;
        } else if (mess.size() >= 5 && mess.substr(0, 5) == "SCORE") {
            score.parse(mess.substr(5, mess.size() - 5), false);
            is_score = true;
        } else if (mess.size() >= 5 && mess.substr(0, 5) == "TOTAL") {
            total.parse(mess.substr(5, mess.size() - 5), true);
            is_total = true;
        }
    }
    string to_message() {
        if (is_iam) return iam.to_message();
        if (is_busy) return busy.to_message();
        if (is_deal) return deal.to_message();
        if (is_trick) return trick.to_message();
        if (is_wrong) return wrong.to_message();
        if (is_taken) return taken.to_message();
        if (is_score) return score.to_message();
        if (is_total) return total.to_message();
        return "";
    }
};

int read_message(int fd, string *buffer, bool is_server);
message parse_message(int fd, string *buffer, string peer_addr, bool is_auto_player = true);
void send_message(int fd, message mess, string peer_addr, bool is_servser,
                  bool is_auto_player = true);
int seat_to_int(char seat);
char int_to_seat(int seat);

#endif