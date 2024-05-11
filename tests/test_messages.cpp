#include <iostream>
#include <string>
#include <netinet/in.h>

#include "common.h"
#include "messages.h"
#include "err.h"
#include <signal.h>

using namespace std;


// GENERATE TEST:
int generate() {
    //cout<<"IAMN\r\n";

    //cout<<"BUSYNARFDG\r\n";

    //cout<<"DEAL4E2SAH10CAHKDQCJS2S\r\n";

    //cout<<"TRICK102S2H3H4H5H6H7H8H9H10HJHQS\r\n";
    //cout<<"TRICK3\r\n";
    //cout<<"TRICK30\r\n";

    //cout<<"WRONG1\r\n";

    //cout<<"TAKEN132H3D10SACE\r\n";
    //cout<<"TAKEN32H3D10SACE\r\n";

    //cout<<"SCOREN0E0S0W0\r\n";
    //cout<<"SCOREN1E2S3W4\r\n";
    //cout<<"SCOREN10E22S33312W432423990\r\n";

    //cout<<"TOTALN0E0S0W0\r\n";
    //cout<<"TOTALN1E2S3W4\r\n";
    //cout<<"TOTALN10E22S33312W432423990\r\n";

}


// TEST:
int main() {
    generate();
    return 0;

    message m = read_message(STDIN_FILENO);

    cout<< "is_im: " << m.is_im << endl;
    cout<< "is_busy: " << m.is_busy << endl;
    cout<< "is_deal: " << m.is_deal << endl;
    cout<< "is_score: " << m.is_score << endl;
    cout<< "is_taken: " << m.is_taken << endl;
    cout<< "is_total: " << m.is_total << endl;
    cout<< "is_trick: " << m.is_trick << endl;
    cout<< "is_wrong: " << m.is_wrong << endl;

    cout<<"\n\n";

    if (m.is_im) {
        cout<< "im: " << m.iam.player << endl;
    }
    if (m.is_busy) {
        cout<< "busy: " << endl;
        for (int i = 0; i < (int)m.busy.players.size(); i++) {
            cout<< m.busy.players[i] << " ";
        }
        cout<< endl;
    }
    if (m.is_deal) {
        cout<< "deal: " << endl;
        cout << "deal_type: " << m.deal.deal_type << endl;
        cout<< "player: " << m.deal.first_player << endl;
        for (int i = 0; i < (int)m.deal.cards.size(); i++) {
            cout<< m.deal.cards[i].value << " " << m.deal.cards[i].color << ";  ";
        }
        cout<< endl;
    }
    if (m.is_trick) {
        cout<< "trick: " << endl;
        cout << "trict number: " << m.trick.trick_number << endl;
        for (int i = 0; i < (int)m.trick.cards.size(); i++) {
            cout<< m.trick.cards[i].value << " " << m.trick.cards[i].color << ";  ";
        }
    }
    if (m.is_wrong) {
        cout<< "wrong: " << m.wrong.trick_number << endl;
    }
    if (m.is_taken) {
        cout<< "taken: " << m.taken.trick_number << endl;
        cout << "player: " << m.taken.player << endl;
        for (int i = 0; i < (int)m.taken.cards.size(); i++) {
            cout<< m.taken.cards[i].value << " " << m.taken.cards[i].color << ";  ";
        }
    }
    if (m.is_score) {
        cout<< "score: " << endl;
        for (int i = 0; i < (int)m.score.scores.size(); i++) {
            cout<< m.score.players[i] << " " << m.score.scores[i] << ";  ";
        }
    }
    if (m.is_total) {
        cout<< "total: " << endl;
        for (int i = 0; i < (int)m.total.scores.size(); i++) {
            cout<< m.total.players[i] << " " << m.total.scores[i] << ";  ";
        }
    }

    return 0;
}