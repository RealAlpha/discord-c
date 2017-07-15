#pragma once
#include "websocket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "cJSON.h"

#include "signal.h"

#include <curl/curl.h>

// Enum to store a user's status
enum memberStatus
{
	MS_Online,
	MS_Idle,
	MS_NotDisturb,
	MS_Offline
};

// Sends a message with "content" as a body to the channel with the "channel" id. Makes the message say out loud is isTTS is true (and the server doesn't ban it)
void sendMessage(/* TODO some kind of connection object?, */char *content, uint64_t channel, uint8_t isTTS);

int client_ws_receive_callback(client_websocket_t *socket, char *data, size_t length);
int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length);
void *heartbeatFunction(void *websocket);
void *thinkFunction(void *websocket);

void handleEventDispatch(client_websocket_t *socket, cJSON *root);
void handleIdentify(client_websocket_t *socket);
void handleOnReady(client_websocket_t *socket, cJSON *root);
void handleGuildMemberChunk(cJSON *root);
void handleMessagePosted(cJSON *root);
void handleMessageUpdated(cJSON *root);
void handlePresenceUpdate(cJSON *root);

/*
// Cleanup
void freeServers(struct server *node);
void freeChannels(struct server_channel *node);
void freeUsers(struct server_user *node);
void freeRoles(struct roles *node);
*/

// Stores the information associated with a connection (token, websocket, etc)
struct connection
{

        // TODO
        client_websocket_t *webSocket;

};

struct user
{
        // Username
        char *username;
        // id
        uint64_t id;
        // TODO Add more fields
};

// Struct that stores a role
struct role
{
        uint64_t id;
        // TODO is uint32_t large enough?
        uint32_t position;
        uint32_t permissions;
        uint32_t color;

        char *name;

        // Boolean!
        uint8_t mentionable;
};

struct roles
{
        struct role *role;

        struct roles *next;
};

struct server_user
{
        // User information
        struct user *user;

        // TODO roles & other server specific stuff
        struct roles *roles;
	
	enum memberStatus status;

        // Linked list next node
        struct server_user *next;
};

struct server_channel
{
        // Channel name
        char *name;
        // Channel topic
        char *topic;
        // Channel id
        uint64_t id;

        // Linked list next node
        struct server_channel *next;
};

struct private_channel
{
	// Recipient's data
	struct user *recipient;

	uint64_t id;
};

// Linked list to store multiple private_channels
struct DM_chat
{
	struct private_channel *channel;

	struct DM_chat *next;
};

struct server
{
        // Server name
        char *name;
        // Server id
        uint64_t serverId;

        // Channels
        struct server_channel *channels;
        // Users
        struct server_user *users;
        // Roles
        struct roles *roles;

        // Linked list next node
        struct server *next;
};

struct message
{
        // Sender of the message
        struct server_user *author;
        // The channel the message got sent in
        struct server_channel *channel;
        // The server the message got sent in
        struct server *server; // TODO is this required? Maybe channel -> server lookup isn't too costly & a better alternative
        // The body of the message
        char *body;
        // (boolean) Has the message been edited?
        uint8_t edited;
};

struct DM_message
{
	// Channel
	struct private_channel *channel;
	// Author
	struct user *author;
	// Text
	char *body;
};

struct messages
{
        // TODO should be pointer instead?
        struct message *message;
        struct messages *next;
};

// Struct to store chunks of messages. Used so the message struct's next will be reliable
struct message_chain
{
        struct messages *chunk;
        struct message_chain *next;
};

typedef void (*discord_login_complete_callback)(struct connection connection, struct server *servers);
typedef void (*discord_memberfetch_complete_callback)(struct server *servers);
typedef void (*discord_message_posted_callback)(struct message message); // TODO should this be a pointer instead? Would that add a ton of overhead t$
typedef void (*discord_message_updated_callback)(struct message message);
typedef void (*discord_presence_updated_callback)(struct server_user *user);
typedef void (*discord_DM_message_posted_callback)(struct DM_message message);
/*
typedef void (*discord_login_complete_callback)(struct connection connection, struct server *servers);
typedef void (*discord_memberfetch_complete_callback)(struct server *servers);
typedef void (*discord_message_posted_callback)(struct message message); // TODO should this be a pointer instead? Would that add a ton of overhead t$
typedef void (*discord_message_updated_callback)(struct message message);
*/

struct discord_callbacks {
        discord_login_complete_callback login_complete;
        discord_memberfetch_complete_callback users_found;
	discord_message_posted_callback message_posted;
        discord_message_updated_callback message_updated;
	discord_presence_updated_callback presence_updated;
	discord_DM_message_posted_callback DM_posted;
//      websocket_connection_error_callback on_connection_error;
};

void cleanup();

// Cleanup
void freeServers(struct server *node);
void freeChannels(struct server_channel *node);
void freeUsers(struct server_user *node);
void freeUserRoles(struct roles *node);
void freeRoles(struct roles *node);
void freeMessageChain(struct message_chain *node);
void freeMessages(struct messages *node);
void freeDMChannels(struct DM_chat *node);

struct messages *getMessagesInChannel(uint64_t channel, int ammount); // TODO create before/around/after functions too?

void finishedRetrievingMembers();

// Sets up a discord client
client_websocket_t *createClient(struct discord_callbacks *callbacks, char *token);

// Loads a guild into memory (fetches users, presences and starts to recieve events such as precense update etc)
void loadGuild(client_websocket_t *socket, uint64_t guildId);
