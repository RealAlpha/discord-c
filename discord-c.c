#include "websocket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "cJSON.h"

#include "signal.h"

int client_ws_receive_callback(client_websocket_t *socket, char *data, size_t length);
int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length);
void *heartbeatFunction(void *websocket);
void *thinkFunction(void *websocket);

void handleEventDispatch(client_websocket_t *socket, cJSON *root);
void handleIdentify(client_websocket_t *socket);
void handleOnReady(client_websocket_t *socket, cJSON *root);
void handleGuildMemberChunk(cJSON *root);

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

// "wrapper" object to go from role -> linked list (for users & server role list). This allows for only storing a role once, instead of potentially multiple thousands of times.
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

typedef void (*discord_login_complete_callback)(struct connection connection, struct server *servers);
typedef void (*discord_memberfetch_complete_callback)(struct server *servers);

struct discord_callbacks {
	discord_login_complete_callback login_complete;
	discord_memberfetch_complete_callback users_found;
//	websocket_connection_error_callback on_connection_error;
};

// Cleanup
void freeServers(struct server *node);
void freeChannels(struct server_channel *node);
void freeUsers(struct server_user *node);
void freeUserRoles(struct roles *node);
void freeRoles(struct roles *node);

// TODO remove - for memory leak stuff test yeah...
client_websocket_t *globWebSocket = NULL;

// Ugly global variable for CLI callbacks
struct discord_callbacks *cli_callbacks = NULL;
// Even more ugly global variable that stores the servers linked list. Needed to go from server id -> server linked list in some functions :(
struct server *glob_servers = NULL;
// Super ugly global variable to store if it's currently awaiting member fragments
uint8_t isRetrievingMembers  = 0;

void finishedRetrievingMembers();

void sigintHandler(int sig)
{
	freeServers(glob_servers);

	if (globWebSocket)
	{
		websocket_disconnect(globWebSocket);
		websocket_free(globWebSocket);
	}

	exit(1);
}

int main(int argc, char *argv[])
{
	signal(SIGINT, sigintHandler);

	printf("Websocket thing starting up!");
	client_websocket_callbacks_t myCallbacks;
	
	myCallbacks.on_receive = client_ws_receive_callback;
	myCallbacks.on_connection_error = client_ws_connection_error_callback;

	client_websocket_t *myWebSocket = malloc(sizeof(client_websocket_t));
	myWebSocket = websocket_create(&myCallbacks);
	globWebSocket = myWebSocket; // TODO remove?
	
	// TODO Make this grab the gateway instead of hard-coding it
	websocket_connect(myWebSocket, "wss://gateway.discord.gg/?v=5&encoding=json");
	
	// Offload websocket_think() to another thread so we can use mutex locks!
	pthread_t serviceThread;
	pthread_create(&serviceThread, NULL, thinkFunction, (void*)myWebSocket);

	// Allow it some time to connect
	//for (int i = 0; i < 10; i++)
	//{
	//	websocket_think(myWebSocket);
	//	sleep(1);
	//}
	
	pthread_t heartbeatThread;
	pthread_create(&heartbeatThread, NULL, heartbeatFunction, (void*)myWebSocket);

	// Keep the main thread occupied so the program doesn't exit
	while(1)
	{
		sleep(1);
	}

	/*
	// Loop to keep the main thread occupied + websocket in service
	while (1)
	{
		/ *
		if (isRetrievingMembers == 0)
		{
			printf("Done retrieving members!\n");
			struct server *server = glob_servers;
			while (server)
			{
				printf("--------------------------------\n%s\n--------------------------------\n", server->name);
				struct server_user *user = server->users;
				while(user)
				{
					printf("Username: %s\nID: %llu\n\n", user->user->username, user->user->id);
					user = user->next;
				}
				server = server->next;
			}
		//	break;
		}
		* /
		websocket_think(myWebSocket);
		//usleep(500*1000);
	}
	*/
	// TODO remove the above "bloat" into functions/threads
	
}

int client_ws_receive_callback(client_websocket_t* socket, char* data, size_t length) {
	// Add a \0 to a copy of the data buffer
	char *buffer = malloc(length + 1);
	strncpy(buffer, data, length);
	buffer[length] = '\0';

	//printf("\n\nRecieved callback!\nContent:\n%s\n\n", data);
	
	// Parse the json using cJSON
	cJSON *root = cJSON_Parse(data);
	
	cJSON *opCodeItem = cJSON_GetObjectItemCaseSensitive(root, "op");

	if(cJSON_IsNumber(opCodeItem))
	{
		int opcode = opCodeItem->valueint;
		printf("Opcode: %i", opcode);
		
		// Is it not an event and is it currently retrieving members? Well stop that, we're done getting those packages!
		if (opcode != 0 && isRetrievingMembers == 1)
			finishedRetrievingMembers();

		switch(opcode)
		{
			case 0:
				// Dispatch
				// Offload handling to dedicated function
				handleEventDispatch(socket, root);
				break;
			case 9:
				// Invalid Session
				// Invalid token / tried to auth too often!
				printf("Invalid session (#9)! Is your token valid?");
				break;
			case 10:
				// Hello
				// Send the identify payload
				handleIdentify(socket);
				break;
			case 11:
				// Heartbeat ACK
				// Don't do anything yet - the "d" doesn't seem to contain any info.
				break;
		}
	}
	// Free the JSON struct thing
	cJSON_Delete(root);
	// Free the buffer copy
	free(buffer);
	return 0;
}

int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length) {
	//discord_client_t* client = (discord_client_t*)websocket_get_userdata(socket);

	printf("Connection error: %s (%zu)\n", reason, length);

	//client_disconnect(client);
	return 0;
}

// Function that keeps repeatively sending a heartbeat
void *heartbeatFunction(void *websocket)
{
	client_websocket_t *myWebSocket = (client_websocket_t*)websocket;

	while (1)
	{
		printf("Sending heartbeat...\n");
		// TODO figure out how to insert the correct d value

		// Create an operation 1 (=heartbeat) packet and send it off
		char *packet = "{\"op\": 1, \"d\": 251}";
		websocket_send(myWebSocket, packet, strlen(packet), 0);
		//websocket_think(myWebSocket);

		// Wait heartbeat interval seconds; TODO make this not hard-coded
		usleep(41250*1000);
	}
}

void *thinkFunction(void *websocket)
{
	client_websocket_t *myWebSocket = (client_websocket_t*)websocket;

	while (1)
		websocket_think(myWebSocket);
}

void handleEventDispatch(client_websocket_t *socket, cJSON *root)
{
	printf("Recieved event displatch!\n");
	
	// Attempt to get the event (the "t" property)
	cJSON *eventNameItem = cJSON_GetObjectItemCaseSensitive(root, "t");
	
	// Try to grab the event's name and call the correct actions based upon the event.
	if(cJSON_IsString(eventNameItem))
	{
		char *eventName = eventNameItem->valuestring;
		
		// Previous chunks where guild members, but the current one isn't. We finished grabbing members! (as it's all done in 1 go)
		if (strcmp(eventName, "GUILD_MEMBERS_CHUNK") != 0 && isRetrievingMembers == 1)
			finishedRetrievingMembers();
		if (strcmp(eventName, "MESSAGE_CREATE") == 0)
		{
			// A new message has been posted
		}
		else if (strcmp(eventName, "READY") == 0)
		{
			// Get's returned after OP IDENTIFY. Recieved information about guilds/users etc & ready to start recieving messages/etc.
			handleOnReady(socket, root);
		}
		else if (strcmp(eventName, "GUILD_MEMBERS_CHUNK") == 0)
		{
			handleGuildMemberChunk(root);
		}
		else
		{
			// Unsupported event!
			printf("Unsupported event! Event: %s\n", eventName);
		}
	}
	else
	{
		// The event wasen't a string, log an error to stderr as it's probably been corrupted
		fprintf(stderr, "Recieved malformed event dispatch!");
	}
}

void handleIdentify(client_websocket_t *socket)
{
	printf("Handling Identification!\n");
	char *request =  "{\"op\":2,\"d\":{\"token\":\"Mjg3MTc2MDM1MTUyMjk3OTg1.DD_c7w.V9NC_tbWiUZYv0jTEGTgyATLl6Q\",\"properties\":{\"$os\":\"linux\",\"$browser\":\"my_library_name\",\"$device\":\"my_library_name\",\"$referrer\":\"\",\"$referring_domain\":\"\"},\"compress\":false,\"large_threshold\":250,\"shard\":[1,10]}}";

	websocket_send(socket, request, strlen(request), 0);

}

void handleOnReady(client_websocket_t *socket, cJSON *root)
{
	// TODO dispatch the heartbeat thread from here instead of in main();
	printf("Successfully established connection!\n");
	
	// Required? Get the data part.
	root = cJSON_GetObjectItemCaseSensitive(root, "d");
	// Get the private messages / DMs
	cJSON *DMs = cJSON_GetObjectItemCaseSensitive(root, "private_channels");
	
	// Get the friends
	cJSON *friends = cJSON_GetObjectItemCaseSensitive(root, "relationships");
	
	// Get the servers the user is in
	cJSON *guilds = cJSON_GetObjectItemCaseSensitive(root, "guilds");
	
	// Request further info about guilds
	cJSON *guild = guilds->child;

	while (guild)
	{
		cJSON *guildNameObject = cJSON_GetObjectItemCaseSensitive(guild, "name");
		cJSON *guildIdObject = cJSON_GetObjectItemCaseSensitive(guild, "id");
		cJSON *guildRolesObject = cJSON_GetObjectItemCaseSensitive(guild, "roles");

		uint64_t guildId = strtoull((const char *)guildIdObject->valuestring, NULL, 10);
		
		if(cJSON_IsNumber(guildIdObject))
		{
			guildId = (uint64_t)guildIdObject->valuedouble;
		}
		else
		{
			printf("Error while processing JSON!\n");
		}
		
		printf("Guild name: %s (id: %lu)\n", guildNameObject->valuestring, guildId);
		
		// Create struct to hold server
		struct server *server = malloc(sizeof(struct server));
		
		server->name = malloc(strlen(guildNameObject->valuestring) + 1);
		strcpy(server->name, guildNameObject->valuestring);
		
		server->serverId = guildId;
		server->channels = NULL;
		server->users = NULL;

		server->next = glob_servers;
		glob_servers = server;
		
		// Set the roles to NULL so the loop can use server->roles for next without worrying about using an uninitialized variable
		server->roles = NULL;

		cJSON *roleObject = guildRolesObject->child;
		while (roleObject)
		{
			uint64_t roleId = strtoull((const char *)cJSON_GetObjectItemCaseSensitive(roleObject, "id")->valuestring, NULL, 10);
			// TODO figure out how on earth this is supposed to represent a color
			uint32_t roleColor = cJSON_GetObjectItemCaseSensitive(roleObject, "color")->valueint;
			char *roleName = cJSON_GetObjectItemCaseSensitive(roleObject, "name")->valuestring;
			// TODO the rest of the role's properties

			struct role *role = malloc(sizeof(struct role));
			struct roles *roleElement = malloc(sizeof(struct roles));

			roleElement->next = server->roles;
			server->roles = roleElement;
			roleElement->role = role;
			role->id = roleId;
			role->color = roleColor;
			role->name = malloc(strlen(roleName) + 1);
			strcpy(role->name, roleName);
			
			// TODO the rest of the properties

			roleObject = roleObject->next;
		}

		// TODO Is 256 chars long enough/too long/etc?
		char packet[256];

		sprintf(packet, "{\"op\":8, \"d\":{\"guild_id\": \"%lu\", \"query\": \"\", \"limit\": 300}}", guildId);
		printf("Request packet: %s\n", packet);
		websocket_send(socket, packet, strlen(packet), 0);
		guild = guild->next;
	}
	
	struct connection connection;
	connection.webSocket = socket;

	// Successfully connected! Callback time
	if (cli_callbacks != NULL && cli_callbacks->login_complete != NULL)
		cli_callbacks->login_complete(connection, glob_servers);

	// TODO Want to free now or later? AKA allow persistence? Posibly required!
	//free(connection);

}

void handleGuildMemberChunk(cJSON *root)
{
	// Go from ugly global varialbe -> local variable
	struct server *servers = glob_servers;
	
	// Set the state to retrieving members
	isRetrievingMembers = 1;

	root = cJSON_GetObjectItemCaseSensitive(root, "d");

	cJSON *guildIdItem = cJSON_GetObjectItemCaseSensitive(root, "guild_id");
	cJSON *membersObject = cJSON_GetObjectItemCaseSensitive(root, "members");
	
	cJSON *memberObject = membersObject->child;
	
	// Grab the guild/server it's id	
	uint64_t guildId = strtoull((const char *)guildIdItem->valuestring, NULL, 10);
	
	struct server *server = NULL;

	if (servers == NULL)
	{
		printf("Error: No servers!\n");
		return;
	}
	
	
	struct server *_server = glob_servers;
	while (_server)
	{
		if (_server->serverId == guildId)
		{
			printf("Found guild!");
			server = _server;
			break;
		}
		_server = _server->next;
	}
	
/*
	for (struct server *_server = servers; _server->next != NULL; _server = _server->next)
	{
		if (server)
		{
			printf("server itteration\n");
			if (_server->serverId == guildId)
			{
				// Found!
				server = _server;
				break;
			}
		}
	}
*/
	if (server == NULL)
	{
		// Couldn't find server? Something went wrong!
		printf("Unable to find server!\n");
	}

	// Linked list to hold the members in this chunk
	struct user *members = NULL;
	
	// Debug counter TODO remove
	int usersAdded = 0;

	while (memberObject)
	{
		cJSON *userObject = cJSON_GetObjectItemCaseSensitive(memberObject, "user");
		cJSON *rolesObject = cJSON_GetObjectItemCaseSensitive(memberObject, "roles");
		cJSON *nicknameObject = cJSON_GetObjectItemCaseSensitive(memberObject, "nick");
		
		cJSON *usernameObject = cJSON_GetObjectItemCaseSensitive(userObject, "username");
		cJSON *userIdObject = cJSON_GetObjectItemCaseSensitive(userObject, "id");

		struct user *user = malloc(sizeof(struct user));
		
		// Allocate username memory + copy it
		user->username = malloc(strlen(usernameObject->valuestring) + 1);
		strcpy(user->username, usernameObject->valuestring);
		
		// Convert the id string into a decimal uint64_t and store it in the id field
		user->id = strtoull((const char *)usernameObject->valuestring, NULL, 10);
		
		// Users' roles
		struct roles *roles = NULL;

		cJSON *roleObject = rolesObject->child;
		
		// Itterate over the roles the user has, pulling out the id and matching it to the server it's role.
		while (roleObject)
		{
			uint64_t roleId = strtoull((const char *)roleObject->valuestring, NULL, 10);
			
			struct roles *role = server->roles;
			while (role)
			{
				if (role->role->id == roleId)
				{
					// Found correct role! Add it to the user roles linked list.
					struct roles *roleElement = malloc(sizeof(struct roles));
					roleElement->next = roles;
					roles = roleElement;
					break;
				}

				role = role->next;
			}

			roleObject = roleObject->next;
		}
		
		struct server_user *userElement = malloc(sizeof(struct server_user));
		
		userElement->user = user;
		userElement->roles = roles;
		
		userElement->next = server->users;
		server->users = userElement;

		memberObject = memberObject->next;
		usersAdded++; // TODO
	}
	
	printf("Recieved member chunk for guild id %lu. Added %i users!\n", guildId, usersAdded);
}

void freeServers(struct server *node)
{
	if (node == NULL)
	{
		return;
	}

	// TODO tail recursion is faster?
	freeServers(node->next);
	
	free(node->name);

	freeChannels(node->channels);

	freeUsers(node->users);
	
	freeRoles(node->roles);

	free(node);
}

// Internal
void freeChannels(struct server_channel *node)
{
	if (node == NULL)
		return;
	
	freeChannels(node->next);

	free(node->name);
	free(node->topic);

	free(node);
}

void freeUsers(struct server_user *node)
{
	if (node == NULL)
		return;
	
	freeUsers(node->next);

	free(node->user->username);
	free(node->user);
	
	freeUserRoles(node->roles);

	free(node);

	// Don't free role! Done by freeRoles()
}

void freeUserRoles(struct roles *node)
{
	if (node == NULL)
		return;

	freeUserRoles(node->next);
	
	free(node);
}

void freeRoles(struct roles *node)
{
	if (node == NULL)
		return;

	freeRoles(node->next);

	free(node->role->name);
	free(node->role);
	
	free(node);
}

void finishedRetrievingMembers()
{
	isRetrievingMembers = 0;
	printf("Finished retrieving members!\n");
	
	// Run callback - TODO uncomment when it's actually set up
	//cli_callbacks->users_found(glob_servers);
}

