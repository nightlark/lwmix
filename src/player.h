//
//  player.h
//  lwmix
//
//

#ifndef lwmix_player_h
#define lwmix_player_h

#include "common.h"
#include "lwmix.h"

typedef struct player {
	int encoded_ip;
	int sernum;
	char * player_info;
	struct scene_node * scene_dest; // scene to send next packet to
	int private_dest; // sernum to send next packet to
	int send_dest; // 0 send to all, 1 send to private, 2 send to scene
    
    char inbuf[MAX_RAW_INPUT_LENGTH]; // buffer for input
    char small_outbuf[SMALL_BUFSIZE]; // output buffer
    // if any of these turn out to be too small, could use linked list of blocks of buffer space
    char *output; // points to the current location of output buffer
    int bufptr; // points to end of current output buffer
    int bufspace; // space remaining in the output buffer
	int socket;
} player;

// Used for lists of players
typedef struct player_node {
	struct player *player;
	struct player_node *next;
} player_node;

// Player related utility functions
player* createPlayer(int);
void addPlayer(player*);
void removePlayer(int);

#endif
