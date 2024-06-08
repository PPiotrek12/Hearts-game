#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "err.h"
#include "game_client.h"
#include "messages.h"

using namespace std;

// Logic of the game - heuristic playing strategy.
Trick play_a_card(shared_ptr<Game_stage_client> game, Trick trick) {
    if (game->act_deal.cards.empty()) exit(1);  // If we don't have any cards, we exit.

    if (trick.cards.empty()) {
        // If we are the first player in the trick, we play the lowest card.
        Card min_card = game->act_deal.cards[0];
        for (Card card : game->act_deal.cards)
            if (card.value < min_card.value) min_card = card;
        Trick res = {.trick_number = trick.trick_number, .cards = {min_card}};
        return res;
    }

    // Select all cards of the same color as the first card in the trick.
    vector<Card> cards_to_color;
    for (Card card : game->act_deal.cards)
        if (card.color == trick.cards[0].color) cards_to_color.push_back(card);

    // If we don't have cards of the same color, we play the highest card.
    if (cards_to_color.empty()) {
        Card max_card = game->act_deal.cards[0];
        for (Card card : game->act_deal.cards)
            if (card.value > max_card.value) max_card = card;
        Trick res = {.trick_number = trick.trick_number, .cards = {max_card}};
        return res;
    }

    // If we have cards of the same color, we will try to put the highest card lower than the
    // highest played. If all out cards-to-color are higher than the highest played, we put the
    // highest card from the same color - we will probably lose the trick.
    Card max_card_from_played = trick.cards[0];
    for (Card card : trick.cards)
        if (card.value > max_card_from_played.value) max_card_from_played = card;

    // Select the highest card lower than the highest played.
    Card max_card_lower = cards_to_color[0];
    max_card_lower.value = -1;
    for (Card card : cards_to_color)
        if (card.value > max_card_lower.value && card.value < max_card_from_played.value)
            max_card_lower = card;

    // If we can't put a card lower than the highest played, we put the highest card from color.
    if (max_card_lower.value == -1) {
        Card max_card_from_color = cards_to_color[0];
        for (Card card : cards_to_color)
            if (card.value < max_card_from_color.value) max_card_from_color = card;
        Trick res = {.trick_number = trick.trick_number, .cards = {max_card_from_color}};
        return res;
    } else {
        Trick res = {.trick_number = trick.trick_number, .cards = {max_card_lower}};
        return res;
    }
}