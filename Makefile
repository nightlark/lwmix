CC = gcc
SRC = src/
LWMIX = $(SRC)lwmix.c
COMMON = $(SRC)common.c
PLAYER = $(SRC)player.c
NETWORK = $(SRC)network.c
MININI = $(SRC)minIni.c

all: lwmix

lwmix:  minIni.o common.o player.o network.o lwmix.o
	$(CC) -o $@ minIni.o common.o player.o network.o lwmix.o -pthread
	rm *.o

lwmix.o: $(LWMIX)
	$(CC) -o $@ -c $(LWMIX)

common.o: $(COMMON)
	$(CC) -o $@ -c $(COMMON)

player.o: $(PLAYER)
	$(CC) -o $@ -c $(PLAYER)

network.o: $(NETWORK)
	$(CC) -o $@ -c $(NETWORK)

minIni.o: $(MININI)
	$(CC) -o $@ -c $(MININI)

