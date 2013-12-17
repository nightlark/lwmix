//
//  common.c
//  lwmix
//
//

#include "common.h"
#include "lwmix.h"
#include "minIni.h"

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

// Loads or creates an lwmix config file if one doesn't exist
void loadLWMIXConfig(const char* file)
{
    // read config file to set up server defaults
	// if no config file found, interactive setup wizard
	
	FILE* config;
	if ((config = fopen(file, "r")) == 0)
    {
		// Run new server admin through setup process
		ini_puts("server", "name", "lwMIX", file);
		ini_puts("server", "port", "8888", file);
		ini_puts("server", "server_rules", "", file);
		ini_putl("server", "id", generateServerID(), file);
		ini_putl("server", "game_id", 0, file);
		ini_puts("server", "game_name", "WoS", file);
		ini_puts("server", "game", "WoS", file);
		ini_puts("server", "info", "", file);
		ini_puts("server", "world", "", file);
		ini_puts("server", "public", "0", file);
	}
    else
	{
        fclose(config);
    }
    
	// Load config information from file
	ini_gets("server", "name", "lwMIX", server_info.name, sizeof(server_info.name), file);
	ini_gets("server", "port", "8888", server_info.port, sizeof(server_info.port), file);
	ini_gets("server", "server_rules", "", server_info.server_rules, sizeof(server_info.server_rules), file);
	server_info.id = (int) ini_getl("server", "id", generateServerID(), file);
	server_info.game_id = (int) ini_getl("server", "game_id", 0, file);
	ini_gets("server", "game_name", "WoS", server_info.game, sizeof(server_info.game), file);
	ini_gets("server", "game", "WoS", server_info.game, sizeof(server_info.game), file);
	ini_gets("server", "info", "", server_info.info, sizeof(server_info.info), file);
	ini_gets("server", "world", "", server_info.world, sizeof(server_info.world), file);
	server_info.public = ini_getbool("server", "public", 0, file);
}