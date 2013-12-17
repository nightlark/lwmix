//
//  common.h
//  lwmix
//
//

#ifndef lwmix_types_h
#define lwmix_types_h

// Time - Needed for a few things
#include <sys/time.h>

// Some overall useful includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

// lwmix Configuration Data

// WoS Master Server
#define MASTER_IP "63.197.64.78"
#define MASTER_PORT "23999"

// Numbers from testing syn-real MIX server
#define MAX_RAW_INPUT_LENGTH 4096
#define SMALL_BUFSIZE 4096

#define CONFIG_FILE "lwmix.cfg"

typedef struct serv_info {
	char name[32];
	char port[16];
	int player_count;
	struct player_node * player_list; // head of the list of all players in the server
    struct player_node * end_player_list; // pointer to the end of the player list
    struct scene_node * scene_list; // list of all the scenes on the server
	char server_rules[128];
	int id;
	int public;
	char * master_ip;
	char * master_port;
	int version_int;
	float version_num;
	int game_id;
	char game[5];
	char host[256];
	char info[128];
	char world[64];
	// anything for limited player online stat tracking?
} serv_info;

// Used for list of rooms created by players, and who is in them; used for mass sending a packet to a subset of the players
typedef struct scene_node {
	int scene_id;
	struct player_node *player_list;
	struct scene_node *next;
} scene_node;

// General server utility functions
int generateServerID();
void loadLWMIXConfig();

#endif
