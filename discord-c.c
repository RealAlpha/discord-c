#include "websocket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "cJSON.h"

int client_ws_receive_callback(client_websocket_t *socket, char *data, size_t length);
int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length);
void *heartbeatFunction(void *websocket);

void handleEventDispatch(cJSON *root);
void handleIdentify(client_websocket_t *socket);
void handleOnReady(cJSON *root);

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

struct server_user
{
	// User information
	struct user user;
	
	// TODO roles & other server specific stuff
	
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
	// Channels
	struct server_channel channels;
	// Users
	struct server_user users;

	// Linked list next node
	struct server *next;
};

typedef void (*discord_login_complete_callback)(struct connection connection, struct server servers[]);
//typedef int (*websocket_connection_error_callback)(client_websocket_t* client, char* reason, size_t length);

struct discord_callbacks {
	discord_login_complete_callback login_complete;
//	websocket_connection_error_callback on_connection_error;
};

// Ugly global variable for CLI callbacks


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
				handleEventDispatch(root);
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

void handleEventDispatch(cJSON *root)
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
			handleOnReady(root);
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

void handleOnReady(cJSON *root)
{
	// TODO dispatch the heartbeat thread from here instead of in main();
	printf("Successfully established connection!\n");
	
}
