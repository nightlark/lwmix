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

#define MAX_RAW_INPUT_LENGTH 4096
#define SMALL_BUFSIZE 4096

#define CONFIG_FILE "lwmaster.cfg"

int running;

// need function to: save and load config files
// need function to: generate packets sent from server
// need function to: handle users
// need function to: handle user requests
// want function to: handle some signals

typedef struct serv_info {
	char name[32];
	char port[16];
	int player_count;
	struct player_node * player_list; // list of all players in the server
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

int createDGRAMSocket(char * addr, char * port, int is_server);
int createTCPSocket(char * addr, char * port, int is_server);
void *masterCheckInTimer(void *serv_config);
void *serverInfoProvider(void *serv_config);

int main (int argc, char *argv[])
{
	// read command line args
	// read config file to set up server defaults
	// if no config file found, interactive setup wizard
	
	serv_info server_info;
	/*server_info.name = "Catch 22";
	server_info.port = "4950";
	server_info.server_rules = "Free For All!!!!";
	server_info.id = generateServerID();
	server_info.game_id = 0;
	server_info.game = "WoS";
	server_info.info = "";
	server_info.world = "Evergreen";*/
	
	int test_config;
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
		ini_putl("server", "public", 1, CONFIG_FILE);
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
	server_info.public = ini_getbool("server", "public", 1, CONFIG_FILE);
	
	/*printf("---------\n%s\n", server_info.name);
	printf("%s\n", server_info.port);
	printf("%s\n", server_info.server_rules);
	printf("%X\n", server_info.id);
	printf("%s\n", server_info.game);
	printf("%d\n---------\n", server_info.public);*/
	
	/*server_info.public = 1;
	server_info.server_rules = "HELP US!";*/
	
	server_info.master_ip = MASTER_IP;
	server_info.master_port = MASTER_PORT;
	server_info.version_int = 0xA124;
	server_info.version_num = 1.10;
	server_info.player_count = 0;
	gethostname(server_info.host, sizeof(server_info.host));
	printf("host name: %s\n", server_info.host);
	server_info.player_list = NULL;
	
	// start threads
	int rc;
	running = 1;
	pthread_t t_serverLoop, t_serverInfo, t_masterCheckIn;
	rc = pthread_create(&t_serverInfo, NULL, *serverInfoProvider, &server_info);
	if (rc) {
		printf("ERROR: return code from pthread_create() is %d\n", rc);
		return -1;
		//exit(-1);
	}
	rc = pthread_create(&t_masterCheckIn, NULL, *masterCheckInTimer, &server_info);
	if (rc) {
		printf("ERROR: return code from pthread_create() is %d\n", rc);
		return -1;
		//exit(-1);
	}
	printf("Started threads\n");
	char a = 0x0;
	while(!a) { a = getchar(); };
	running = 0;
	pthread_exit(NULL);
	// watch for signals sent to process (via command line or other)
	// make changes based on signals
	// write config changes out to file
	// save SSVs
	// shut down threads
	// exit
}

void *masterCheckInTimer(void *serv_config)
{
	// code to checkin with the master periodically
	serv_info *server_info;
	server_info = (serv_info*) serv_config;
	if (server_info->public) {
		// Create UDP socket
		int bytes_sent, total_bytes_sent;
		unsigned char buffer[1024];
		int len;
		int i = 0;
		int master_sock = createDGRAMSocket(server_info->master_ip, server_info->master_port, 0);
		printf("Master started \n");
		while (server_info->public){
			// Formulate UDP packet
			// if behind a router, should sent 0 as the port
			len = snprintf(buffer, sizeof(buffer), "!version=%u,nump=%u,gameid=%u,game=%s,host=%s,id=%X,port=%s,info=%s,name=%s",
						  server_info->version_int, server_info->player_count, server_info->game_id, server_info->game,
						  server_info->host, server_info->id, server_info->port, server_info->info, server_info->name);
			len++; // null-character automatically added
			bytes_sent = 0;
			// Send UDP packet
			bytes_sent = send(master_sock, buffer, len, 0);
			if (bytes_sent < 0) {
				printf("[ERROR] Could not send CheckIn Packet (%d)\n", errno);
				
			}
			printf("Master CheckIn Sent\n");
			len = recv(master_sock, buffer, sizeof(buffer), 0);
			// bytes 4-7 are ip address received by master, in network order
			// bytes 8-9 are port received at, in little endian form
			/*for (i = 0; i < 16; i++) {
				printf("%02X", buffer[i]);
			}
			printf("\nlength %u: %s\n", len, buffer);*/
			// Can get our real ip address from this
			// Also, if behind a firewall, we might need to use the port number this gives us
			
			sleep(300); // Sleep for 5 minutes... might want to replace with something that can be interrupted
			// maybe use a timer?
		};
		buffer[0] = 0x58;
		buffer[1] = 0x00;
		len = 2;
		send(master_sock, buffer, len, 0);
		close(master_sock);
		// Send UDP packet
		printf("Master stopped \n");
	}
}

void *serverInfoProvider(void *serv_config)
{	
	fd_set master;
	fd_set read_fds;
	int fdmax;
	int i;
	int bytes_sent;
	int bytes_recv;
	struct sockaddr_storage their_addr;
	socklen_t addr_len = sizeof(their_addr);
	
	// provides users information needed to connect
	serv_info *server_info;
	server_info = (serv_info*) serv_config;
	
	// Create UDP listener socket
	int info_sock = createDGRAMSocket(NULL, server_info->port, 1);
	char buffer[1024];
	int len;
	int send_response;
	struct sockaddr_in * sin;
	unsigned char * ip;
	
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
					case '!':
						// Server check-in
						// Packet received:
						/*
						 len = snprintf(buffer, sizeof(buffer), "!version=%u,nump=%u,gameid=%u,
									   game=%s,host=%s,id=%X,port=%s,info=%s,name=%s",
									   server_info->version_int, server_info->player_count, 
									   server_info->game_id, server_info->game,
									   server_info->host, server_info->id, server_info->port, 
									   server_info->info, server_info->name);
						 len++; // null-character automatically added
						 */
						
						// Send response
						// bytes 4-7 are ip address received by master, in network order
						// bytes 8-9 are port received at, in little endian form
						len = snprintf(buffer, sizeof(buffer), "#name=%s [world=%s] //Rules:%s //ID:%X //TM:%X //US:1.1.26",
									  server_info->name, server_info->world, server_info->server_rules, server_info->id, (unsigned int) time(NULL));
						len++;
						send_response = 1;
						break;
					case 0x58:
						// Server check-out
						
						send_response = 0;
						break;
					case '?':
						// Server list request
						// Receives:
						/*
						 QByteArray* playerInfo = new QByteArray("?alias=" + lineEditAlias->text().toAscii() +
						 ",name=" + lineEditName->text().toAscii() +
						 ",email=" + lineEditEMail->text().toAscii() +
						 ",loc=" + lineEditLocation->text().toAscii() +
						 ",sernum=" + lineEditSoulID->text().toAscii() +
						 ",HHMM=" + lineEditHHMM->text().toAscii() +
						 ",d=" + lineEditD->text().toAscii() +
						 ",v=" + lineEditV->text().toAscii() +
						 ",w=" + lineEditW->text().toAscii());
						 playerInfo->append((char) 0x00);
						 */
						
						// Send response:
						// If its a response from the master with a list of servers:
						// first two bytes should be 02 02; will be 03 03 if you're "bad"?
						// next two match dotted MIX version num, reversed 24 a1
						// next two bytes is the number of mix servers online
						// next two bytes are always 0c 00
						// next 4 bytes give our ip address, in normal order
						// next is the listing of the servers
						// -first 4 bytes are the ip address in normal order
						// -next 4 bytes are the port number, reversed
						// -next 2 bytes are the number of online players
						// -next byte is the lower part of the mix server version number
						// -end with a null byte
						send_response = 1;
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
	};
	close(info_sock);
	printf("Info stopped \n");
}

int generateServerID()
{
	srand(time(NULL));
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