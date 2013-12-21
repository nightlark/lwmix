
// Threading library
#include <pthread.h>

// Sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>

// lwmix includes
#include "common.h"
#include "lwmix.h"
#include "player.h"
#include "network.h"

int running;
serv_info server_info;
int udp_sock = NULL;

// TODO: replace checkin loop with checkin timer
// TODO: server info provider should log sernum of most recent request from each ip address, use it for when they connect
// TODO: handle player sending sernum info after connecting
// TODO: parse MIX packets directed towards the server
// TODO: send MIX packets
// TODO: conditional sending of packets
// need function to: manipulate SSVs -> just use ini files, saving and loading taken care of by minIni
// need function to: store player information; probably save it to disk as it comes in
// need function to: encrypt player ip address
// need function to: generate packets sent from server
// need function to: handle users
// need function to: handle user requests
// want function to: get master ip and port
// want function to: handle some signals

// Threads for different server functions that happen in parallel
void *masterCheckInTimer(void *serv_config);
void *serverInfoProvider(void *serv_config);
void *serverLoop(void *serv_config);

int main (int argc, char *argv[])
{
	// read command line args for special instructions
    loadLWMIXConfig(CONFIG_FILE);
	
    // Initialize some basic info on this server
	server_info.master_ip = MASTER_IP;
	server_info.master_port = MASTER_PORT;
	server_info.version_int = 0xA124;
	server_info.version_num = 1.10;
	server_info.player_count = 0;
    server_info.player_list = NULL;
    
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
    char buffer[1024];
    
    // Create UDP socket for checking in with the master server
    //int master_sock = createDGRAMSocket(server_info.master_ip, server_info.master_port, 0);
    int master_sock;
    while (udp_sock == NULL);
    master_sock = udp_sock;
    
    if (server_info.public)
    {
        printf("Master check-in timer started \n");
    }
    
    while (running) {
        if (server_info.public) {
            masterCheckIn(master_sock, buffer, sizeof(buffer));
			
			sleep(300); // Sleep for 5 minutes... might want to replace with something that can be interrupted
			// maybe use a timer?
		}
    }

    // Close the socket
    close(master_sock);

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
    
    char* token;
    
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
    udp_sock = info_sock;
	
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
				if (bytes_recv > 1) {
                    switch (buffer[0]) {
                        case 'P':
                            // Ex: Palias=Newb,name=Newbie,email=unknown,loc=unknown,sernum=SOUL 5000,HHMM=13:37,d=00000000,v=00000000,w=00000000
                            // Extract info from packet
                            token = strtok(&buffer[1], ",");
                            while (token != NULL)
                            {
                                printf("Requestee Info: %s\n", token);
                                if (strncmp(token, "sernum=SOUL ", 12) == 0)
                                {
                                    printf("Found the sernum: %s\n", &token[7]);
                                }
                                token = strtok(NULL, ",");
                            }
                            
                            // Send server info
                            if (strncmp(server_info.world, "", sizeof(server_info.world)))
                            {
                                len = snprintf(buffer, sizeof(buffer), "#name=%s [world=%s] //Rules: %s //ID:%X //TM:%X //US:1.1.26",
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
                            // Ex: QCA1,CD5,C9C,5F1621B2,8E2,9A6,B12,CD4,
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
	
    player* temp_player;
    
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
                        temp_player = createPlayer(newfd);
                        addPlayer(temp_player);
                        temp_player->sernum = server_info.player_count*0x123;
                        // TODO: Send SR@I, SR@M packets
                        // SR@I with all players sent to connecting player
                        // SR@I with just new player sent to all others
					}
				} else {
					if ((recv_bytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (recv_bytes == 0) {
							printf("Disconnected\n");
                            removePlayer(i);
						} else {
							perror("Error receiving packet\n");
						}
						close(i);
						FD_CLR(i, &master);
					} else {
                        printf("Packet Received: %s\n", buffer);
                        if (strncmp(buffer, ":MIX", 4) == 0)
                        {
                            // TODO: Handle packets directed at mix server
                            printf("Got MIX packet\n");
                        }
                        else
                        {
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
		}
		
		// Process connects, disconnects
		// Process incoming packets; handle if starts with :MIX
		// Process outgoing packets
	};
	printf ("Server stopped \n");
    
    return NULL;
}