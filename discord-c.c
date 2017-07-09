#include "websocket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "cJSON.h"

int client_ws_receive_callback(client_websocket_t *socket, char *data, size_t length);
int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length);
void *heartbeatFunction(void *websocket);

void handleEventDispatch(client_websocket_t *socket, cJSON *root);
void handleIdentify(client_websocket_t *socket);
void handleOnReady(client_websocket_t *socket, cJSON *root);
void handleGuildMemberChunk(cJSON *root);

// Stores the information associated with a connection (token, websocket, etc)
struct connection
{

	// TODO
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

typedef void (*discord_login_complete_callback)(struct connection connection, struct server servers);
//typedef int (*websocket_connection_error_callback)(client_websocket_t* client, char* reason, size_t length);

struct discord_callbacks {
	discord_login_complete_callback login_complete;
//	websocket_connection_error_callback on_connection_error;
};

// Ugly global variable for CLI callbacks
struct discord_callbacks cli_callbacks;
// Even more ugly global variable that stores the servers linked list. Needed to go from server id -> server linked list in some functions :(
struct server *glob_servers = NULL;

int main(int argc, char *argv[])
{
	printf("Websocket thing starting up!");
	client_websocket_callbacks_t myCallbacks;
	
	myCallbacks.on_receive = client_ws_receive_callback;
	myCallbacks.on_connection_error = client_ws_connection_error_callback;

	client_websocket_t *myWebSocket = malloc(sizeof(client_websocket_t));
       	myWebSocket = websocket_create(&myCallbacks);
	
	// TODO Make this grab the gateway instead of hard-coding it
	//if (argc != 2)
	websocket_connect(myWebSocket, "wss://gateway.discord.gg/?v=5&encoding=json");
	//sleep(20);
	for (int i = 0; i < 10; i++)
	{
		websocket_think(myWebSocket);
		sleep(1);
	}
	
	pthread_t heartbeatThread;
	pthread_create(&heartbeatThread, NULL, heartbeatFunction, (void*)myWebSocket);
	
	// Loop to keep the main thread occupied + websocket in service
	while (1)
	{
		websocket_think(myWebSocket);
		usleep(500*1000);
	}
}

int client_ws_receive_callback(client_websocket_t* socket, char* data, size_t length) {
	// Add a \0 to a copy of the data buffer
	char *buffer = malloc(length + 1);
	strncpy(buffer, data, length);
	buffer[length] = '\0';

	printf("\n\nRecieved callback!\nContent:\n%s\n\n", data);
	
	// Parse the json using cJSON
	cJSON *root = cJSON_Parse(data);
	
	cJSON *opCodeItem = cJSON_GetObjectItemCaseSensitive(root, "op");

	if(cJSON_IsNumber(opCodeItem))
	{
		int opcode = opCodeItem->valueint;
		printf("Opcode: %i", opcode);
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

void handleEventDispatch(client_websocket_t *socket, cJSON *root)
{
	printf("Recieved event displatch!\n");
	
	// Attempt to get the event (the "t" property)
	cJSON *eventNameItem = cJSON_GetObjectItemCaseSensitive(root, "t");
	
	// Try to grab the event's name and call the correct actions based upon the event.
	if(cJSON_IsString(eventNameItem))
	{
		char *eventName = eventNameItem->valuestring;
		
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
			roleElement->role = role;
			role->id = roleId;
			role->color = roleColor;
			role->name = roleName;
			// TODO the rest of the properties

			roleObject = roleObject->next;
		}

		// TODO Is 256 chars long enough/too long/etc?
		char packet[256];

		sprintf(packet, "{\"op\":8, \"d\":{\"guild_id\": \"%lu\", \"query\": \"\", \"limit\": 0}}", guildId);
		printf("Request packet: %s\n", packet);
		websocket_send(socket, packet, strlen(packet), 0);
		guild = guild->next;
	}

}

void handleGuildMemberChunk(cJSON *root)
{
	// Go from ugly global varialbe -> local variable
	struct server *servers = glob_servers;

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
	struct user *members;

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
	}
	
	printf("Recieved member chunk for guild id %lu", guildId);
}
