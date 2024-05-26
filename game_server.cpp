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
#include <fcntl.h>
#include <algorithm>
#include <chrono>
#include <sys/time.h>

#include "common.h"
#include "messages.h"
#include "err.h"
#include "game_server.h"

int find_looser_and_update_scores(shared_ptr<Game_stage_server> game) {
    int looser = 0;
    Trick act_trick = game->act_trick;
    for (int i = 1; i < (int)act_trick.cards.size(); i++) {
        if (act_trick.cards[i].color == act_trick.cards[looser].color &&
                act_trick.cards[i].value > act_trick.cards[looser].value)
            looser = i;
    }
    looser = (looser + game->act_trick.act_player + 1) % 4;
    int p[8] = {0, 1, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < (int)act_trick.cards.size(); i++) {
        if (act_trick.cards[i].color == 'H') p[2]++;
        if (act_trick.cards[i].value == 12) p[3] += 5;
        if (act_trick.cards[i].value == 11 || act_trick.cards[i].value == 13) p[4] += 2;
        if (act_trick.cards[i].value == 13 && act_trick.cards[i].color == 'H') p[5] += 18;
    }
    if (act_trick.trick_number == 7 || act_trick.trick_number == 13) p[6] = 10;
    p[7] = p[1] + p[2] + p[3] + p[4] + p[5] + p[6];
    game->total_scores.scores[looser] += p[game->act_deal.deal_type];
    game->act_deal.scores.scores[looser] += p[game->act_deal.deal_type];
    return looser;
}