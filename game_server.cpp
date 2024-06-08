#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "err.h"
#include "game_server.h"
#include "messages.h"

// Returns 1 if player chose correct card, 0 otherwise.
// If the trick is correct then update the game state.
bool is_trick_correct(shared_ptr<Game_stage_server> game, Trick trick, int player) {
    if (trick.cards.size() != 1) return 0;
    if (trick.trick_number != game->act_trick.trick_number) return 0;
    Card played_card = trick.cards[0];
    vector<Card> possible_to_play;
    if (game->act_trick.cards.empty())
        possible_to_play = game->act_deal.deals[player].cards;
    else {
        char color = game->act_trick.cards[0].color;
        for (Card card : game->act_deal.deals[player].cards)
            if (card.color == color) possible_to_play.push_back(card);
        if (possible_to_play.empty()) possible_to_play = game->act_deal.deals[player].cards;
    }
    bool played_correctly = false;
    for (int i = 0; i < (int)possible_to_play.size(); i++) {
        Card act_card = possible_to_play[i];
        if (act_card.value == played_card.value && act_card.color == played_card.color)
            played_correctly = true;
    }
    if (!played_correctly) return 0;
    // If the card is the played correctly then remove it from the player's hand.
    for (int i = 0; i < (int)game->act_deal.deals[player].cards.size(); i++) {
        Card act_card = game->act_deal.deals[player].cards[i];
        if (act_card.value == played_card.value && act_card.color == played_card.color) {
            game->act_deal.deals[player].cards.erase(game->act_deal.deals[player].cards.begin() +
                                                     i);
            game->act_trick.cards.push_back(played_card);
            game->act_trick.how_many_played++;
            break;
        }
    }
    return 1;
}

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