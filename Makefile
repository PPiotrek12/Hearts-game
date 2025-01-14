CC = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++20

.PHONY: all clean

TARGETS = kierki-klient kierki-serwer

all: $(TARGETS)

kierki-klient: kierki-klient.o common.o err.o messages.o game_client.o
kierki-serwer: kierki-serwer.o common.o err.o messages.o game_server.o

common.o: common.cpp err.h common.h
err.o: err.cpp err.h
game_client.o: game_client.cpp common.h messages.h err.h game_client.h
game_server.o: game_server.cpp common.h messages.h err.h game_server.h
kierki-klient.o: kierki-klient.cpp common.h messages.h err.h game_client.h
kierki-serwer.o: kierki-serwer.cpp common.h messages.h err.h
messages.o: messages.cpp messages.h common.h err.h

clean:
	rm -f *.o $(TARGETS)
