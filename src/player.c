//
//  player.c
//  lwmix
//
//

#include "player.h"

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