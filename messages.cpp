#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>

#include "messages.h"
#include "common.h"
#include "err.h"

using namespace std;

message read_message(int fd) {
    char act;
    string mess;
    do {
        if (readn(fd, &act, 1) < 0)
            syserr("read");
        mess += act;
    } while (act != '\n');
    mess = mess.substr(0, mess.size() - 2);
    cout<<mess<<endl;
    message res;
    res.parse(mess);
    return res;
}

void send_message(int fd, message mess) {
    string to_send = mess.to_message();
    if (writen(fd, (char*)to_send.c_str(), to_send.size()) < 0)
        syserr("write");
}