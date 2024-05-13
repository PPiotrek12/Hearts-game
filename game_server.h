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

struct Deal_format {
    int deal_type;
    int first_player; // N=0, E=1, S=2, W=3
    Deal deal[4];
    void parse(string header, string list1, string list2, string list3, string list4) {
        map <char, int> seat_to_int = {{'N', 0}, {'E', 1}, {'S', 2}, {'W', 3}};
        deal_type = header[0] - '0';
        first_player = seat_to_int[header[1]];
        deal[0].parse(header + list1);
        deal[1].parse(header + list2);
        deal[2].parse(header + list3);
        deal[3].parse(header + list4);
    }
};

struct Game_format {
    vector <Deal_format> deals;
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
            Deal_format deal;
            deal.parse(header, line1, line2, line3, line4);
            deals.push_back(deal);
        }
    }
};


#endif