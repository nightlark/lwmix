CC = gcc
LWMIX = lwmix.c
MININI = minIni.c
MININIH = minIni.h
MINGLUEH = minGlue.h

all: lwmix

lwmix: minIni.o lwmix.o
	$(CC) -o $@ minIni.o lwmix.o

lwmix.o: $(LWMIX)
	$(CC) -o $@ -c $(LWMIX)

minIni.o: $(MININIH) $(MINGLUEH) $(MININI)
	$(CC) -o $@ -c $(MININI)

