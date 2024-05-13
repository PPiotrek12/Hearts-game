#include <iostream>

#include "common.h"
#include "messages.h"
#include "err.h"
#include "game_server.h"

using namespace std;

void parse_arguments(int argc, char* argv[], uint16_t *port, string *file, int *timeout) {
    *timeout = 5;
    bool wasFileSet = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-p") {
            if (i + 1 >= argc) fatal("missing argument for -p");
            else {
                *port = read_port(argv[i + 1]);
                i++;
            }
        } 
        else if (arg == "-f") {
            if (i + 1 >= argc) fatal("missing argument for -f");
            else {
                *file = argv[i + 1];
                wasFileSet = true;
                i++;
            }
        }
        else if (arg == "-t") {
            if (i + 1 >= argc) fatal("missing argument for -t");
            else {
                *timeout = stoi(argv[i + 1]);
                i++;
            }
        }
        else fatal("unknown argument");
    }
    if (!wasFileSet) fatal("missing -f argument");
}

int main(int argc, char* argv[]) {
    uint16_t port;
    string file;
    int timeout;
    parse_arguments(argc, argv, &port, &file, &timeout);
    Game_format game_format;
    game_format.parse(file);
}