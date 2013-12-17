//
//  network.c
//  lwmix
//
//

#include "network.h"

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