//
//  network.c
//  lwmix
//
//

#include "network.h"

void masterCheckOut(int master_sock, char* buffer)
{
    ssize_t len;
    
    // Checkout packet
    buffer[0] = 0x58;
    buffer[1] = 0x00;
    len = 2;
    
    send(master_sock, buffer, len, 0);
    
    printf("Master CheckOut sent \n");
}

void masterCheckIn(int master_sock, char* buffer)
{
    ssize_t bytes_sent;
    ssize_t len;
    
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