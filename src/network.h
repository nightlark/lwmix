//
//  network.h
//  lwmix
//

#ifndef lwmix_network_h
#define lwmix_network_h

// Sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>

#include "common.h"

// may be good to write function to create sockets for you.. create socket(ip, port, type)
// may also be good to write function to process partial incoming packets
// may also be good to process outgoing packets

// Networking helper functions
int createDGRAMSocket(char * addr, char * port, int is_server);
int createTCPSocket(char * addr, char * port, int is_server);

#endif
