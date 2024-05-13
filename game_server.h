#ifndef MIM_GAME_SERVER_H
#define MIM_GAME_SERVER_H

#include <iostream>
#include <string>
#include <netinet/in.h>
#include <map>
#include <vector>
#include <fstream>

#include "common.h"
#include "messages.h"
#include "err.h"

using namespace std;

// Struct representing a specific trick for all players.
struct Trick_server: Trick {
    bool played[4] = {false, false, false, false};
    int how_many_played = 0;
    int act_player = 0;
};

// Stuct representing a specific deal for all players.
struct Deal_server {
    int deal_type = 0;
    int first_player = 0; // N=0, E=1, S=2, W=3
    int points[4] = {0, 0, 0, 0};
    Deal deals[4];
    void parse(string header, string list1, string list2, string list3, string list4) {
        map <char, int> seat_to_int = {{'N', 0}, {'E', 1}, {'S', 2}, {'W', 3}};
        deal_type = header[0] - '0';
        first_player = seat_to_int[header[1]];
        deals[0].parse(header + list1);
        deals[1].parse(header + list2);
        deals[2].parse(header + list3);
        deals[3].parse(header + list4);
    }
};

struct Game_stage_server {
    bool occupied[4] = {false, false, false, false};
    int how_many_occupied = 0;
    int total_scores[4] = {0, 0, 0, 0};
    int deal_number = -1;
    Deal_server act_deal;
    Trick_server act_trick;
    vector <Taken> all_taken;
};

// Struct representing all deals in the game read from the file.
struct Game_scenario {
    vector <Deal_server> deals;
    void parse(string file) {
        ifstream file_stream;
        file_stream.open(file);
        if (!file_stream.is_open()) fatal("cannot open file");
        while (!file_stream.eof()) {
            string header, line1, line2, line3, line4;
            getline(file_stream, header);
            if (header == "") break;
            getline(file_stream, line1);
            if (line1 == "") break;
            getline(file_stream, line2);
            if (line2 == "") break;
            getline(file_stream, line3);
            if (line3 == "") break;
            getline(file_stream, line4);
            if (line4 == "") break;
            Deal_server deal;
            deal.parse(header, line1, line2, line3, line4);
            deals.push_back(deal);
        }
    }
};

#endif