CC = gcc
SRC = src/
LWMIX = $(SRC)lwmix.c
MININI = $(SRC)minIni.c

all: lwmix

lwmix:  minIni.o lwmix.o
	$(CC) -o $@ minIni.o lwmix.o
	rm *.o

lwmix.o: $(LWMIX)
	$(CC) -o $@ -c $(LWMIX)

minIni.o: $(MININI)
	$(CC) -o $@ -c $(MININI)

