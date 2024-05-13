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
#include "game_client.h"

using namespace std;

// Logic of the game.
Trick play_a_card(shared_ptr<Game> game, Trick trick) { // TODO: implement it better.
    if (game->is_auto_playes) {
        Trick res;
        res.trick_number = trick.trick_number;
        res.cards.push_back(game->act_deal.cards.back());
        return res;
    }
    cout << "Choose a card to play: ";
    string input;
    while (true) {
        cin >> input; // TODO: co jesli w tym miejscu uzytkownik zapyta o cards albo tricks?
        if (input.size() < 3 || input[0] != '!' || 
                !Card::is_card_correct(input.substr(1, input.size() - 1))) {
            cout << "Wrong card format, try again: ";
            continue;
        }
        else break;
    }
    Trick res;
    res.trick_number = trick.trick_number;
    res.cards.push_back(Card(input.substr(1, input.size() - 1)));
    return res;
}

// TODO: co w przypadku gdy jestesmy zawieszeni na cin, a serwer wysyla nam
// wiadomosc - czy mamy ja pozniej odebrac i moze sie okazac ze byla (jest) poprawna? - 
// - tak na prawde pytanie czy czekanie na karte w osobnym cin jest poprawne.