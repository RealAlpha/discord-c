#include "websocket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "cJSON.h"

#include "signal.h"

#include <curl/curl.h>
#include "discord-c.h"

// Global thread variables
pthread_t serviceThread;
pthread_t heartbeatThread;

//struct messages *getMessagesInChannel(uint64_t channel, int ammount); // TODO create before/around/after functions too?

// TODO remove - for memory leak stuff test yeah...
client_websocket_t *globWebSocket = NULL;

// Ugly global variable for CLI callbacks
struct discord_callbacks *cli_callbacks = NULL;
// Even more ugly global variable that stores the servers linked list. Needed to go from server id -> server linked list in some functions :(
struct server *glob_servers = NULL;
// Really ugly "message chain" for freeing messages
struct message_chain *message_chain = NULL;

// Super ugly global variable to store if it's currently awaiting member fragments
uint8_t isRetrievingMembers  = 0;

int sequenceNumber = 0;

//void finishedRetrievingMembers();

void sigintHandler(int sig)
{
	cleanup();
	exit(1);
}

void cleanup()
{
	freeServers(glob_servers);
	freeMessageChain(message_chain);

	// Kill the threads (TODO could this cause issues?)
	pthread_cancel(serviceThread);
	pthread_cancel(heartbeatThread);

	if (globWebSocket)
	{
		websocket_disconnect(globWebSocket);
		websocket_free(globWebSocket);
	}
}

int main(int argc, char *argv[])
{
	signal(SIGINT, sigintHandler);
	fprintf(stderr, "Discord-c starting up!");
	
	createClient();

	// Test send message
	sleep(20);
	sendMessage("Hello, world!", 332535524869013505, 0);
	getMessagesInChannel(332535524869013505, 10);

	// Keep the main thread occupied so the program doesn't exit
	while(1)
	{
		sleep(1);
	}

}

void createClient()
{
	client_websocket_callbacks_t *myCallbacks = malloc(sizeof(client_websocket_callbacks_t));
	
	myCallbacks->on_receive = client_ws_receive_callback;
	myCallbacks->on_connection_error = client_ws_connection_error_callback;

	client_websocket_t *myWebSocket = NULL;//malloc(sizeof(client_websocket_t));
	myWebSocket = websocket_create(myCallbacks);
	globWebSocket = myWebSocket; // TODO remove?
	sleep(10);
	// TODO Make this grab the gateway instead of hard-coding it
	websocket_connect(myWebSocket, "wss://gateway.discord.gg/?v=5&encoding=json");
	printf("Just tried to connect");

	sleep(10);
	
	printf("Creating threads...");
	// Offload websocket_think() to another thread so we can use mutex locks!
	//pthread_t serviceThread;
	pthread_create(&serviceThread, NULL, thinkFunction, (void*)myWebSocket);
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
	
	// Set the last sequence
	cJSON *sequenceNumberItem = cJSON_GetObjectItemCaseSensitive(root, "s");
	if (sequenceNumberItem)
		sequenceNumber = sequenceNumberItem->valueint;

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
		// Wait heartbeat interval seconds; TODO make this not hard-coded
		usleep(41250*1000);

		printf("Sending heartbeat...\n");
		// TODO figure out how to insert the correct d value

		// Create an operation 1 (=heartbeat) packet and send it off
		// 25  chars to be safe
		char packet[128];
		sprintf(packet, "{\"op\": 1, \"d\": %i}", sequenceNumber);
		websocket_send(myWebSocket, packet, strlen(packet), 0);
		//websocket_think(myWebSocket);
	}
}

void *thinkFunction(void *websocket)
{
	client_websocket_t *myWebSocket = (client_websocket_t*)websocket;

	while (1)
	{
		//printf("Think Loop!\n");
		websocket_think(myWebSocket);
		//usleep(200000);
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
		
		// Previous chunks where guild members, but the current one isn't. We finished grabbing members! (as it's all done in 1 go)
		if (strcmp(eventName, "GUILD_MEMBERS_CHUNK") != 0 && isRetrievingMembers == 1)
			finishedRetrievingMembers();
		if (strcmp(eventName, "MESSAGE_CREATE") == 0)
		{
			// A new message has been posted
			handleMessagePosted(root);
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
		else if(strcmp(eventName, "MESSAGE_ACK") == 0)
		{
			// Don't do anything yet, this event seems broken?

		}
		else if(strcmp(eventName, "MESSAGE_UPDATE") == 0)
		{
			// Message has been updated
			handleMessageUpdated(root);
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
		cJSON *guildChannelsObject = cJSON_GetObjectItemCaseSensitive(guild, "channels");

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
		
		// Avoid garbage values / not intitialized errors
		server->channels = NULL;
		
		cJSON *channelObject = guildChannelsObject->child;
		while(channelObject)
		{
			uint64_t channelId = strtoull((const char *)cJSON_GetObjectItemCaseSensitive(channelObject, "id")->valuestring, NULL, 10);
			
			cJSON *channelNameObject = cJSON_GetObjectItemCaseSensitive(channelObject, "name");
			char *channelName = malloc(strlen(channelNameObject->valuestring) + 1);
			strcpy(channelName, channelNameObject->valuestring);

			cJSON *channelTopicObject = cJSON_GetObjectItemCaseSensitive(channelObject, "topic");
			char *channelTopic = NULL;
			
			if (channelTopicObject == NULL || channelTopicObject->valuestring == NULL)
			{
				channelTopic = malloc(sizeof(char));
				channelTopic[0] = '\0';
			}
			else
			{
				channelTopic = malloc(strlen(channelTopicObject->valuestring) + 1);
				strcpy(channelTopic, channelTopicObject->valuestring);
			}

			struct server_channel *channel = malloc(sizeof(struct server_channel));
			channel->id = channelId;
			channel->name = channelName;
			channel->topic = channelTopic;

			channel->next = server->channels;
			server->channels = channel;

			channelObject = channelObject->next;
		}

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

		sprintf(packet, "{\"op\":8, \"d\":{\"guild_id\": \"%lu\", \"query\": \"\", \"limit\": 0}}", guildId);
		printf("Request packet: %s\n", packet);
		websocket_send(socket, packet, strlen(packet), 0);
		guild = guild->next;
	}
	
	struct connection connection;
	connection.webSocket = socket;
	// Spawn the heartbeat thread
	pthread_create(&heartbeatThread, NULL, heartbeatFunction, (void*)socket);
	
	// Successfully connected! Callback time
	if (cli_callbacks != NULL && cli_callbacks->login_complete != NULL)
		cli_callbacks->login_complete(connection, glob_servers);

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
		user->id = strtoull((const char *)userIdObject->valuestring, NULL, 10);

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

void freeMessages(struct messages *node)
{
	if (node == NULL)
		return;

	freeMessages(node->next);

	free(node->message->body);
	free(node->message);

	free(node);
}

void freeMessageChain(struct message_chain *node)
{
	if (node == NULL)
		return;
	
	freeMessageChain(node->next);
	
	freeMessages(node->chunk);

	free(node);
}

void finishedRetrievingMembers()
{
	isRetrievingMembers = 0;
	printf("Finished retrieving members!\n");
	
	// Run callback - TODO uncomment when it's actually set up
	if (!cli_callbacks || !cli_callbacks->users_found)
		printf("You didn't set up the users found callback!\n");
	else
		cli_callbacks->users_found(glob_servers);
}

void handleMessagePosted(cJSON *root)
{
	root = cJSON_GetObjectItemCaseSensitive(root, "d");

	cJSON *authorObject = cJSON_GetObjectItemCaseSensitive(root, "author");
	cJSON *contentObject = cJSON_GetObjectItemCaseSensitive(root, "content");
	cJSON *channelIdObject = cJSON_GetObjectItemCaseSensitive(root, "channel_id");

	// TODO add webhook handling + DM handling
	cJSON *userIdObject = cJSON_GetObjectItemCaseSensitive(authorObject, "id");

	uint64_t userId = strtoull((const char *)userIdObject->valuestring, NULL, 10);
	uint64_t channelId = strtoull((const char *)channelIdObject->valuestring, NULL, 10);

	
	// Attempt to find the correct server/channel
	struct server *server = glob_servers;
	struct server_channel *channel = NULL;
	
	while(server)
	{
		// Did it already find the channel? (AKA not eqaul to NULL)
		if (channel)
			break;

		printf("Server id: %lu|Server name: %s\n", server->serverId, server->name);
		struct server_channel *_channel = server->channels;
		while (_channel)
		{
			//printf("Channel id: %llu|Required id: %llu|Channel Name: %s", _channel->id, channelId, _channel->name);
			if (channelId == _channel->id)
			{
				printf("Found channel with name: %s\n", _channel->name);
				channel = _channel;
				break;
			}
			
			_channel = _channel->next;
		}
		
		// Here again so it doesn't switch to next...helps preserve the server variable
		if (channel)
			break;

		server = server->next;
	}

	if(server == NULL || channel == NULL)
	{
		printf("Error while recieving message! User not in channel!\n");
		return;
	}
	
	
	// Try to find the user.
	struct server_user *user = server->users;
	while (user)
	{
		//printf("Currently itterating over user: %s (%llu). Required: %llu\n", user->user->username, user->user->id, userId);
		if (user->user->id == userId)
		{
			// Found user! (no need to copy it as breaking here will keep user where it currently is)
			break;
		}

		user = user->next;
	}

	if (user == NULL)
	{
		printf("Unable to find author of the message!\n");
		return;
	}

	// Stuff it all in a struct
	struct message message;
	message.author = user; // TODO should be user->user instead?
	message.channel = channel;
	message.server = server;
	message.body = contentObject->valuestring; // TODO strcopy it instead (current solution gets freed once this function retruns)? If so, make a linked list of messages (message-chain) so it can easily be freed.

	printf("New message:\n%s\n(by: %s (%lu) in %s/%s)\n", message.body, message.author->user->username, message.author->user->id, message.server->name, message.channel->name);

	// Call the cli back with this message
	if (!cli_callbacks || !cli_callbacks->message_posted)
		fprintf(stderr, "You didn't set up the message posted callback!\n");
	else
		cli_callbacks->message_posted(message);
}

void handleMessageUpdated(cJSON *root)
{
	root = cJSON_GetObjectItemCaseSensitive(root, "d");
	
	if (!root)
	{
		fprintf(stderr, "Something went wrong while handling message update! (root JSON object is invalid)\n");
		return;
	}
	cJSON *authorObject = cJSON_GetObjectItemCaseSensitive(root, "author");
	cJSON *contentObject = cJSON_GetObjectItemCaseSensitive(root, "content");
	cJSON *channelIdObject = cJSON_GetObjectItemCaseSensitive(root, "channel_id");

	// TODO add webhook handling + DM handling
	cJSON *userIdObject = cJSON_GetObjectItemCaseSensitive(authorObject, "id");
	
	if (!userIdObject || !channelIdObject || !authorObject || !contentObject)
	{
		// Something went wrong!
		fprintf(stderr, "Something went wrong while handling message update! (not all JSON filled out)\n");
		fprintf(stderr, "JSON: %s", cJSON_Print(root));
		return;
	}
	
	uint64_t userId = strtoull((const char *)userIdObject->valuestring, NULL, 10);
	uint64_t channelId = strtoull((const char *)channelIdObject->valuestring, NULL, 10);

	
	// Attempt to find the correct server/channel
	struct server *server = glob_servers;
	struct server_channel *channel = NULL;
	
	while(server)
	{
		// Did it already find the channel? (AKA not eqaul to NULL)
		if (channel)
			break;

		printf("Server id: %lu|Server name: %s\n", server->serverId, server->name);
		struct server_channel *_channel = server->channels;
		while (_channel)
		{
			//printf("Channel id: %llu|Required id: %llu|Channel Name: %s", _channel->id, channelId, _channel->name);
			if (channelId == _channel->id)
			{
				printf("Found channel with name: %s\n", _channel->name);
				channel = _channel;
				break;
			}
			
			_channel = _channel->next;
		}
		
		// Here again so it doesn't switch to next...helps preserve the server variable
		if (channel)
			break;

		server = server->next;
	}

	if(server == NULL || channel == NULL)
	{
		printf("Error while recieving message! User not in channel!\n");
		return;
	}
	
	
	// Try to find the user.
	struct server_user *user = server->users;
	while (user)
	{
		//printf("Currently itterating over user: %s (%llu). Required: %llu\n", user->user->username, user->user->id, userId);
		if (user->user->id == userId)
		{
			// Found user! (no need to copy it as breaking here will keep user where it currently is)
			break;
		}

		user = user->next;
	}

	if (user == NULL)
	{
		printf("Unable to find author of the message!\n");
		return;
	}

	// Stuff it all in a struct
	struct message message;
	message.author = user;
	message.channel = channel;
	message.server = server;
	message.body = contentObject->valuestring; // TODO strcopy it instead (current solution gets freed once this function retruns)? If so, make a linked list of messages (message-chain) so it can easily be freed.

	printf("Message Updated:\n%s\n(by: %s (%lu) in %s/%s)\n", message.body, message.author->user->username, message.author->user->id, message.server->name, message.channel->name);
	
	// Call the cli back
	if (!cli_callbacks || !cli_callbacks->message_updated)
		fprintf(stderr, "You didn't set up the message update callback!\n");
	else
		cli_callbacks->message_updated(message);
}

void sendMessage(/* TODO some kind of connection object?, */char *content, uint64_t channel, uint8_t isTTS)
{
	// Creeate a curl "object"
	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURL *curl = curl_easy_init();
	if (curl) {
		// Create a JSON object to hold the message
		cJSON *root = cJSON_CreateObject();
		// TODO Serialize username tags -> userid tags (eg. @ImLolly#3232 -><@57023780SOMID7352>) & clamp input to 2k characters
		cJSON_AddItemToObject(root, "content", cJSON_CreateString(content));

		// Convert channel id into a messages url (TODO evaluate the connect ammount of characters)
		char channelMessagesLink[85];
		sprintf(channelMessagesLink, "https://discordapp.com/api/v6/channels/%lu/messages", channel);

		// Set the tts parameter to true or false
		if (isTTS)
			cJSON_AddTrueToObject(root, "tts");
		else
			cJSON_AddFalseToObject(root, "tts");
		// Put into a char * so it can be manually freed; Not handled by cJSON_Delete_()
		char *jsonPayload = cJSON_Print(root);

		// Set the channel it needs  to be sent to
		curl_easy_setopt(curl, CURLOPT_URL, channelMessagesLink);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload);
		
		// Set up the headers to successfully talk with the discord api
		struct curl_slist *list = NULL;
		list = curl_slist_append(list, "authorization: Mjg3MTc2MDM1MTUyMjk3OTg1.DEe6LA.ZhC1yv2MPGb5Y6z-Rph4wdSzKG0");
		list = curl_slist_append(list, "Accept: application/json");
		list = curl_slist_append(list, "content-type: application/json");
		list = curl_slist_append(list, "User-Agent: DiscordBot (null, v0.0.1)");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

		// Attempt to make the request
		CURLcode res;
		res = curl_easy_perform(curl);

		// Cleanup time!
		cJSON_Delete(root);
		free(jsonPayload);
		curl_slist_free_all(list);
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
}

struct string {
  char *ptr;
  size_t len;
};

void init_string(struct string *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;
}

struct messages *getMessagesInChannel(uint64_t channel, int amount)
{
	if (amount > 100 || amount < 0)
	{
		fprintf(stderr, "Invalid ammount of messages requested, resorting to default (50) Please use 0-100");
		amount = 50;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);
	CURL *curl = curl_easy_init();
	struct messages *messages = NULL;

	if (curl) {
		// TODO make this buffer the correct size
		char requestUri[80];
		sprintf(requestUri, "https://discordapp.com/api/v6/channels/%ld/messages?limit=%i", channel, amount);
		curl_easy_setopt(curl, CURLOPT_URL, requestUri);

		// Add HTTP headers to correctly communicate with the discord API
		struct curl_slist *list = NULL;

		list = curl_slist_append(list, "authorization: Mjg3MTc2MDM1MTUyMjk3OTg1.DEe6LA.ZhC1yv2MPGb5Y6z-Rph4wdSzKG0");
		list = curl_slist_append(list, "Accept: application/json");
		list = curl_slist_append(list, "content-type: application/json");
		list = curl_slist_append(list, "User-Agent: DiscordBot (null, v0.0.1)");

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		
		// Set up the result holding
		struct string s;
		init_string(&s);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
		
		// (attempt to) perform the request.
		CURLcode res;
		res = curl_easy_perform(curl);
		
		cJSON *root = cJSON_Parse(s.ptr);
		
		if (!root->child)
			return NULL;

		cJSON *channelIdObject = cJSON_GetObjectItemCaseSensitive(root->child, "channel_id");

		uint64_t channelId = strtoull((const char *)channelIdObject->valuestring, NULL, 10);


		// Attempt to find the correct server/channel - as this is a get messages in channel call it shouldn't change!
		struct server *server = glob_servers;
		struct server_channel *channel = NULL;

		while(server)
		{
			// Did it already find the channel? (AKA not eqaul to NULL)
			if (channel)
				break;

			printf("Server id: %lu|Server name: %s\n", server->serverId, server->name);
			struct server_channel *_channel = server->channels;
			while (_channel)
			{
				//printf("Channel id: %llu|Required id: %llu|Channel Name: %s", _channel->id, channelId, _channel->name);
				if (channelId == _channel->id)
				{
					printf("Found channel with name: %s\n", _channel->name);
					channel = _channel;
					break;
				}

				_channel = _channel->next;
			}

			// Here again so it doesn't switch to next...helps preserve the server variable
			if (channel)
				break;

			server = server->next;
		}

		if(server == NULL || channel == NULL)
		{
			printf("Error while recieving message! User not in channel!\n");
			return NULL;
		}


		// Itterate over the messages;
		cJSON *message = root->child;

		while (message)
		{
			cJSON *authorObject = cJSON_GetObjectItemCaseSensitive(message, "author");
			cJSON *contentObject = cJSON_GetObjectItemCaseSensitive(message, "content");
			// TODO add webhook handling + DM handling
			cJSON *userIdObject = cJSON_GetObjectItemCaseSensitive(authorObject, "id");
		
			uint64_t userId = strtoull((const char *)userIdObject->valuestring, NULL, 10);

			// Try to find the user.
			struct server_user *user = server->users;
			while (user)
			{
				//printf("Currently itterating over user: %s (%llu). Required: %llu\n", user->user->username, user->user->id, userId);
				if (user->user->id == userId)
				{
					// Found user! (no need to copy it as breaking here will keep user where it currently is)
					break;
				}

				user = user->next;
			}

			if (user == NULL)
			{
				printf("Unable to find author of the message!\n");
				return NULL;
			}


			// Stuff it all in a struct
			struct message *_message = malloc(sizeof(struct message));
			_message->author = user;
			_message->channel = channel;
			_message->server = server;
			_message->body = malloc(strlen(contentObject->valuestring) + 1);
			strcpy(_message->body, contentObject->valuestring);

			struct messages *__message = malloc(sizeof(struct messages));

			__message->message = _message;

			__message->next = messages;
			messages = __message;

			message = message->next;
		}
		// Cleanup time!
		curl_easy_cleanup(curl);
		cJSON_Delete(root);
		free(s.ptr);
	}
	curl_global_cleanup();
	
	// Add this set of messages to the message chain (for free-ing)
	struct message_chain *messageEntry = malloc(sizeof(struct message_chain));
	messageEntry->chunk = messages;
	messageEntry->next = message_chain;
	message_chain = messageEntry;
	return messages;
}
