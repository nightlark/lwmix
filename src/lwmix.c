#include <pthread.h>

// Time - Needed for certain features
#include <sys/time.h>

// Sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>

// Useful stuff
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

// minIni
#include "minIni.h"

// WoS Master Server
#define MASTER_IP "63.197.64.78"
#define MASTER_PORT "23999"

// Numbers from testing syn-real MIX server
#define MAX_RAW_INPUT_LENGTH 4096
#define SMALL_BUFSIZE 4096

#define CONFIG_FILE "lwmix.cfg"

// TODO: break this file into multiple smaller files
// TODO: replace checkin loop with checkin timer
// TODO: add players based on when they connect (or disconnect)
// TODO: handle player sending sernum info
// TODO: parse MIX packets directed towards the server
// TODO: conditional sending of packets
// need function to: manipulate SSVs -> just use ini files, saving and loading taken care of by minIni
// need function to: store player information; probably save it to disk as it comes in
// need function to: encrypt player ip address
// need function to: generate packets sent from server
// need function to: handle users
// need function to: handle user requests
// want function to: get master ip and port
// want function to: handle some signals

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

// Used for lists of players
typedef struct player_node {
	struct player *player;
	struct player_node *next;
} player_node;

// Used for list of rooms created by players, and who is in them; used for mass sending a packet to a subset of the players
typedef struct scene_node {
	int scene_id;
	struct player_node *player_list;
	struct scene_node *next;
} scene_node;

int running;
serv_info server_info;

// Player related utility functions
player* createPlayer(int);
void addPlayer(player*);
void removePlayer(int);

// General server utility functions
int generateServerID();

// Networking helper functions
int createDGRAMSocket(char * addr, char * port, int is_server);
int createTCPSocket(char * addr, char * port, int is_server);

// Threads for different server functions that happen in parallel
void *masterCheckInTimer(void *serv_config);
void *serverInfoProvider(void *serv_config);
void *serverLoop(void *serv_config);

int main (int argc, char *argv[])
{
	// read command line args
	// read config file to set up server defaults
	// if no config file found, interactive setup wizard
	
	FILE* test_config;
	if ((test_config = fopen(CONFIG_FILE, "r")) == 0) {
		// Run new server admin through setup process
		ini_puts("server", "name", "lwMIX", CONFIG_FILE);
		ini_puts("server", "port", "8888", CONFIG_FILE);
		ini_puts("server", "server_rules", "", CONFIG_FILE);
		ini_putl("server", "id", generateServerID(), CONFIG_FILE);
		ini_putl("server", "game_id", 0, CONFIG_FILE);
		ini_puts("server", "game_name", "WoS", CONFIG_FILE);
		ini_puts("server", "game", "WoS", CONFIG_FILE);
		ini_puts("server", "info", "", CONFIG_FILE);
		ini_puts("server", "world", "", CONFIG_FILE);
		ini_puts("server", "public", "0", CONFIG_FILE);
	}
	
	// Load config information from file
	ini_gets("server", "name", "lwMIX", server_info.name, sizeof(server_info.name), CONFIG_FILE);
	ini_gets("server", "port", "8888", server_info.port, sizeof(server_info.port), CONFIG_FILE);
	ini_gets("server", "server_rules", "", server_info.server_rules, sizeof(server_info.server_rules), CONFIG_FILE);
	server_info.id = (int) ini_getl("server", "id", generateServerID(), CONFIG_FILE);
	server_info.game_id = (int) ini_getl("server", "game_id", 0, CONFIG_FILE);
	ini_gets("server", "game_name", "WoS", server_info.game, sizeof(server_info.game), CONFIG_FILE);
	ini_gets("server", "game", "WoS", server_info.game, sizeof(server_info.game), CONFIG_FILE);
	ini_gets("server", "info", "", server_info.info, sizeof(server_info.info), CONFIG_FILE);
	ini_gets("server", "world", "", server_info.world, sizeof(server_info.world), CONFIG_FILE);
	server_info.public = ini_getbool("server", "public", 0, CONFIG_FILE);
	
    
    // Initialize some basic info on this server
	server_info.master_ip = MASTER_IP;
	server_info.master_port = MASTER_PORT;
	server_info.version_int = 0xA124;
	server_info.version_num = 1.10;
	server_info.player_count = 0;
    server_info.player_list = NULL;
    
    // Add some test players to the system
    addPlayer(createPlayer(0xCA1));
    addPlayer(createPlayer(0x5F1621B2));
    addPlayer(createPlayer(0xA59));
    addPlayer(createPlayer(0xABC));
    
    
    // Getting the system host name
	gethostname(server_info.host, sizeof(server_info.host));
	printf("Host Name: %s\n", server_info.host);
	
	// start threads
	int rc;
	running = 1;
	pthread_t t_serverLoop, t_serverInfo, t_masterCheckIn;
    
	rc = pthread_create(&t_serverLoop, NULL, &serverLoop, NULL);
	if (rc) {
        perror(NULL);
		return -1;
	}
    
	rc = pthread_create(&t_serverInfo, NULL, &serverInfoProvider, NULL);
	if (rc) {
		perror(NULL);
        pthread_join(t_serverLoop, NULL);
		return -1;
	}

	rc = pthread_create(&t_masterCheckIn, NULL, &masterCheckInTimer, NULL);
	if (rc) {
		perror(NULL);
        pthread_join(t_serverLoop, NULL);
        pthread_join(t_serverInfo, NULL);
		return -1;
	}
    
	printf("Started threads\n");
    
	char a = 0x0;
	while(!a) { a = getchar(); };
    
	running = 0;
    pthread_join(t_serverLoop, NULL);
    pthread_join(t_serverInfo, NULL);
    pthread_join(t_masterCheckIn, NULL);
	pthread_exit(NULL);
    
	// watch for signals sent to process (via command line or other)
	// make changes based on signals
	// write config changes out to file
	// save SSVs
	// shut down threads
	// exit
}

void *masterCheckInTimer(void *arg)
{
	if (server_info.public) {
		ssize_t bytes_sent;
		char buffer[1024];
		ssize_t len;
        
        // Create UDP socket for checking in with the master server
		int master_sock = createDGRAMSocket(server_info.master_ip, server_info.master_port, 0);
        
		printf("Master check-in timer started \n");
        
		while (server_info.public){
			// Formulate UDP packet
			// if behind a router, should sent 0 as the port
			len = snprintf(buffer, sizeof(buffer), "!version=%u,nump=%u,gameid=%u,game=%s,host=%s,id=%X,port=%s,info=%s,name=%s",
						  server_info.version_int, server_info.player_count, server_info.game_id, server_info.game,
						  server_info.host, server_info.id, server_info.port, server_info.info, server_info.name);
			len++; // null-character automatically added
			bytes_sent = 0;
            
			// Send UDP packet
			bytes_sent = send(master_sock, buffer, len, 0);
			if (bytes_sent < 0) {
				printf("[ERROR] Could not send CheckIn Packet (%d)\n", errno);
				
			}
            
			printf("Master CheckIn Sent\n");
			len = recv(master_sock, buffer, sizeof(buffer), 0);
            
            /* Received packet format:
             bytes 4-7 are ip address received by master, in network order
             bytes 8-9 are port received at, in little endian form
             This can be used to get our real ip address, and also the port number if we are behind a firewall
             */
			
			sleep(300); // Sleep for 5 minutes... might want to replace with something that can be interrupted
			// maybe use a timer?
		};
        
        // Checkout packet
		buffer[0] = 0x58;
		buffer[1] = 0x00;
		len = 2;
        
		send(master_sock, buffer, len, 0);
        
        // Close the socket
		close(master_sock);
        
		printf("Master stopped \n");
	}
    
    return NULL;
}

void *serverInfoProvider(void *arg)
{	
	fd_set master;
	fd_set read_fds;
	int fdmax;
    
	int i;
    int characters_written;
    char player_list_buffer[576];
    char player_list_size;
    
    // Player list iterator
    player_node *it_player;
    
    // Networking related variables
	ssize_t bytes_sent;
	ssize_t bytes_recv;
	struct sockaddr_storage their_addr;
	socklen_t addr_len = sizeof(their_addr);
	struct sockaddr_in * sin;
	unsigned char * ip;
    
	// Create UDP listener socket
	int info_sock = createDGRAMSocket(NULL, server_info.port, 1);
	char buffer[1024];
	int len;
	int send_response;
    
	
	FD_SET(info_sock, &master);
	fdmax = info_sock;
	printf("Info started \n");
	while (running) {
		read_fds = master;
		select(fdmax+1, &read_fds, NULL, NULL, NULL);
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				bytes_recv = recvfrom(i, buffer, sizeof(buffer), 0, (struct sockaddr *)&their_addr, &addr_len);
				send_response = 0;
				
				switch (buffer[0]) {
					case 'P':
						// Send server info
                        if (strncmp(server_info.world, "", sizeof(server_info.world)))
                        {
                            len = snprintf(buffer, sizeof(buffer), "#name=%s [world=%s] //Rules:%s //ID:%X //TM:%X //US:1.1.26",
                                           server_info.name, server_info.world, server_info.server_rules, server_info.id, (unsigned int) time(NULL));
                        }
                        else
                        {
                            len = snprintf(buffer, sizeof(buffer), "#name=%s //Rules:%s //ID:%X //TM:%X //US:1.1.26",
                                           server_info.name, server_info.server_rules, server_info.id, (unsigned int) time(NULL));
                        }
						len++;
						send_response = 1;
						break;
					case 'Q':
						// Send comma separated list of players
						// QCA1,CD5,C9C,5F1621B2,8E2,9A6,B12,CD4,
                        player_list_size = 0;
                        characters_written = 0;
                        player_list_buffer[0] = 0;
                        it_player = server_info.player_list;
                        while (it_player != NULL)
                        {
                            printf("Sernum: %X\n",it_player->player->sernum);
                            characters_written = snprintf(player_list_buffer+player_list_size,
                                     sizeof(player_list_buffer) - player_list_size,
                                     "%X,",
                                     it_player->player->sernum);
                            if (characters_written >= 0)
                            {
                                player_list_size += characters_written;
                            }
                            it_player = it_player->next;
                        }
						len = snprintf(buffer, sizeof(buffer), "Q%s", player_list_buffer);
						len++;
						send_response = 1;
						break;
					case 'G':
						// Player is telling us which world they selected
						send_response = 0;
						break;
					default:
						printf("[ERROR] Unknown Packet Type: %s\n", buffer);
						send_response = 0;
						break;
				}
				
				if (send_response) {
					sin = (struct sockaddr_in *)&their_addr;
					bytes_sent = sendto(info_sock, buffer, len, 0, sin, sizeof(struct sockaddr_in));
					if (bytes_sent < 0) {
						printf("[ERROR] Packet Send Failed %d (%s) %d\n", errno, buffer, len);
						ip = (unsigned char *)&sin->sin_addr.s_addr;
						printf("UDP Packet IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
					}
				}
			}
		}
		// Listen for incoming connections/packets
		// Check if from master server ip
		// Send proper response
	}
	close(info_sock);
	printf("Info stopped \n");
    
    return NULL;
}


void *serverLoop(void *serv_config)
{
	fd_set master, read_fds;
	int fdmax;
	int listener;
	int newfd;
	int i, j;
	char buffer[4096];
	ssize_t recv_bytes;
	
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	
	listener = createTCPSocket(NULL, server_info.port, 1);
	if (listen(listener, 10) == -1) {
		perror("listen fail\n");
		exit(-1);
	}
	
	FD_SET(listener, &master);
	fdmax = listener;
	
	// provides service for the players
	printf("Server started \n");
	while (running) {
		// Want to use select to get packets, will save processor
		read_fds = master;
		select(fdmax+1, &read_fds, NULL, NULL, NULL);
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener) {
					addrlen = sizeof(remoteaddr);
					newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);
					
					if (newfd == -1) {
						perror("Failed to accept connection\n");
					} else {
						FD_SET(newfd, &master);
						if (newfd > fdmax) {
							fdmax = newfd;
						}
						printf("New connection\n");
					}
				} else {
					if ((recv_bytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (recv_bytes == 0) {
							printf("Disconnected\n");
						} else {
							perror("Error receiving packet\n");
						}
						close(i);
						FD_CLR(i, &master);
					} else {
						for (j = 0; j <= fdmax; j++) {
							if (FD_ISSET(j, &master)) {
								if (j != listener && j != i) {
									if (send(j, buffer, recv_bytes, 0) == -1) {
										perror("Send failed\n");
									}
								}
							}
						}
					}
				}
			}
		}
		
		// Process connects, disconnects
		// Process incoming packets; handle if starts with :MIX
		// Process outgoing packets
	};
	printf ("Server stopped \n");
    
    return NULL;
}

// Create a new struct for a player and fill out their info
player* createPlayer(int sernum)
{
    player* new = malloc(sizeof(player));
    new->sernum = sernum;
    return new;
}

// Add a player to the list of players
void addPlayer(player* p)
{
    // Create a node for the player
    if (server_info.player_count == 0)
    {
        // Create a new list head for the first player
        server_info.player_list = malloc(sizeof(player_node));
        
        // Set the pointer to the last player in the list
        server_info.end_player_list = server_info.player_list;
    }
    else
    {
        // Create space for the new player at the end of the player list
        server_info.end_player_list->next = malloc(sizeof(player_node));
        
        // Set the pointer to the end of the player list
        server_info.end_player_list = server_info.end_player_list->next;
    }
    
    // Set the player data for this list entry
    server_info.end_player_list->player = p;
    
    // Set next entry to NULL (otherwise bad stuff can happen)
    server_info.end_player_list->next = NULL;
    
    // Increase player count by 1
    server_info.player_count++;
}

// Remove a player from the list of players based on sernum
void removePlayer(int sernum)
{
    player_node* prev_player = NULL;
    player_node* it_player = server_info.player_list;
    player* found_player = NULL;
    
    // Base case: no players
    if (it_player == NULL)
    {
        return;
    }
    
    // Otherwise, start searching names until we reach the end of the list, or the player is found
    while (it_player != NULL && found_player == NULL)
    {
        if (sernum == it_player->player->sernum)
        {
            // Yipee! We found our player.
            found_player = it_player->player;
        }
        else
        {
            // Not the right player, move on to the next one
            prev_player = it_player;
            it_player = it_player->next;
        }
    }
    
    // If the player was found, remove them from the player list
    if (found_player)
    {
        // If the player we are removing is the head of the list, there is no previous player
        if (prev_player == NULL)
        {
            // Update list head to skip first player
            server_info.player_list = server_info.player_list->next;
            
            // If there are no other players, destroy the head
            if (server_info.player_list == NULL)
            {
                free(server_info.player_list);
            }
        }
        else
        {
            // Skip over the player we are removing in the list
            prev_player->next = it_player->next;
        }
        
        // Free the player_node and player data
        free(it_player);
        free(found_player);
        
        // Decrease the player count
        server_info.player_count--;
    }
}

// Generates an ID for the server
int generateServerID()
{
	srand((unsigned int) time(NULL));
	int id = rand();
	id = id << 4;
	id = id ^ rand();
	id = id << 4;
	id = id ^ (rand() << 10);
	id = id ^ rand();
	id = id << 4;
	id = id ^ (rand() << 10);
	id = id ^ rand();
	return id;
}

// may be good to write function to create sockets for you.. create socket(ip, port, type)
// may also be good to write function to process partial incoming packets
// may also be good to process outgoing packets

int createTCPSocket(char * addr, char * port, int is_server) {
	struct addrinfo hints, *res, *p;
	int status;
	int sockfd;
	int yes = 1;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if (is_server) {
		hints.ai_flags = AI_PASSIVE;
	}
	
	if ((status = getaddrinfo(addr, port, &hints, &res)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}
	
	for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1) {
            perror("unable to get a socket\n");
            continue;
        }
		
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("unable to set socket options\n");
            exit(1);
        }
		
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind fail\n");
            continue;
        }
		
        break;
    }
	
	if (p == NULL) {
		printf("no addresses found\n");
		freeaddrinfo(res);
		return -1;
	}
	
	freeaddrinfo(res);
	
	return sockfd;
}

int createDGRAMSocket(char * addr, char * port, int is_server) {
	struct addrinfo hints, *res, *p;
	int status;
	int sockfd;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // We only want IPv4; I doubt the games this is for even support IPv6
	hints.ai_socktype = SOCK_DGRAM;
	if (is_server) {
		hints.ai_flags = AI_PASSIVE;	
	}
	
	if ((status = getaddrinfo(addr, port, &hints, &res)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}
	
	for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			printf("unable to get socket descriptor\n");
            continue;
        }
		
		if (is_server) {
			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				printf("unable to bind socket\n");
				continue;
			}
		}
		
		if (!is_server) {
			if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				printf("unable to connect\n");
				continue;
			}
		}
		
        break;
    }
	
	if (p == NULL) {
		printf("no addresses found\n");
		freeaddrinfo(res);
		return -1;
	}
	
	freeaddrinfo(res);
    
	return sockfd;
}