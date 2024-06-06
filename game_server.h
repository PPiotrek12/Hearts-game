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

// Wrapper of pollfd structure with timeout in milliseconds and buffer for reading messages.
struct Timeout_fd {
    int fd;
    int timeout; // In milliseconds, -1 if no timeout.
    int revents;
    string buffer;
};

// Structure for listening on multiple file descriptors with timeouts. It holds a vector of clients
// and a file descriptor for accepting new connections. It has a method wrapped_poll that is a
// poll which ends after the shortest timeout of all clients. It updates timeouts and revents.
// Additionally it has a parameter game_stopped which is used to not poll players when it is set 1.
struct Listener {
    vector <Timeout_fd> clients; // First 4 clients are players, the rest are not.
    Timeout_fd accepts;
    void wrapped_poll(bool game_stopped = false) {
        pollfd fds[clients.size() + 1];
        fds[0] = {.fd = accepts.fd, .events = POLLIN, .revents = 0};
        int min_timeout = -1;
        for (int i = 0; i < (int)clients.size(); i++) {
            if (game_stopped && i < 4) // Do not poll players when game stopped.
                fds[i + 1] = {.fd = -1, .events = 0, .revents = 0}; 
            else {
                fds[i + 1] = {.fd = clients[i].fd, .events = POLLIN, .revents = 0};
                if (clients[i].timeout >= 0) {
                    if (min_timeout == -1) min_timeout = clients[i].timeout;
                    else min_timeout = min(min_timeout, clients[i].timeout);
                }
            }
        }
        for (int i = 0; i < (int)clients.size() + 1; i++) { 
            if (fds[i].fd != -1 && fcntl(fds[i].fd, F_SETFL, O_NONBLOCK) < 0)
                syserr("fcntl"); // Set non-blocking mode.
            fds[i].revents = 0;
        }

        // Start measuring time.
        auto beg_time = chrono::system_clock::now();

        int poll_status = poll(fds, clients.size() + 1, min_timeout);
        if (poll_status < 0) syserr("poll");

        // End measuring time and calculate duration in milliseconds.
        auto end_time = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = end_time - beg_time;
        int milliseconds = elapsed_seconds.count() * 1000;

        // Update timeouts and revents.
        accepts.revents = fds[0].revents;
        for (int i = 0; i < (int)clients.size(); i++) {
            if (game_stopped && i < 4) continue; // Do not update players when game stopped.
            if (clients[i].timeout != -1) clients[i].timeout -= milliseconds;
            clients[i].revents = fds[i + 1].revents;
        }
    }
};

// Struct representing a specific trick for all players.
struct Trick_server: Trick {
    int how_many_played = 0, act_player = -1;
    void send_trick(int fd) {
        message mess = {.trick = *this, .is_trick = true};
        send_message(fd, mess);
    }
    void send_wrong(int fd) {
        message mess = {.wrong = {.trick_number = trick_number}, .is_wrong = true};
        send_message(fd, mess);
    }
};

// Stuct representing a specific deal for all players.
struct Deal_server {
    int deal_type = 0;
    int first_player = 0; // N=0, E=1, S=2, W=3
    Score scores;
    Deal deals[4];
    void parse(string header, string list1, string list2, string list3, string list4) {
        deal_type = header[0] - '0';
        first_player = seat_to_int(header[1]);
        deals[0].parse(header + list1);
        deals[1].parse(header + list2);
        deals[2].parse(header + list3);
        deals[3].parse(header + list4);
    }
    void send_deals(shared_ptr<Listener> listener) {
        for (int i = 0; i < 4; i++) {
            message mess = {.deal = deals[i], .is_deal = true};
            send_message(listener->clients[i].fd, mess);
        }
    }
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

struct Game_stage_server {
    bool occupied[4] = {false, false, false, false};
    int how_many_occupied = 0;

    bool game_stopped = false, game_started = false;
    int timeout;
    Score total_scores = {{0, 0, 0, 0}, {'N', 'E', 'S', 'W'}, true};
    int deal_number = -1;
    Deal_server act_deal;
    Trick_server act_trick;
    vector <Taken> all_taken;    

    Game_scenario game_scenario;

    void send_busy(int fd) {
        Busy busy;
        vector <char> seats;
        for (int i = 0; i < 4; i++)
            if (occupied[i]) seats.push_back(int_to_seat(i));
        busy.players = seats;
        message mess = {.busy = busy, .is_busy = true};
        send_message(fd, mess);
    }

    void send_all_taken(int fd) {
        for (int i = 0; i < (int)all_taken.size(); i++) {
            message mess = {.taken = all_taken[i], .is_taken = true};
            send_message(fd, mess);
        }
    }
};

int find_looser_and_update_scores(shared_ptr<Game_stage_server> game);
bool is_trick_correct(shared_ptr<Game_stage_server> game, Trick trick, int player);
#endif