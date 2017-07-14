#include "discord-c.h"
#include "stdio.h"
#include "signal.h"

void onMessagePostedCallback(struct message message);
void onReadyCallback(struct connection connection, struct server *server);

void sigintHandler(int sig)
{
	cleanup();
	exit(1);
}

int main(int argc, char *argv[])
{
	signal(SIGINT, sigintHandler);
	fprintf(stderr, "Discord-c starting up!");
	
	struct discord_callbacks callbacks;
	
	callbacks.login_complete = onReadyCallback;
	callbacks.users_found = NULL;
	callbacks.message_posted = onMessagePostedCallback;
	callbacks.message_updated = NULL;
	callbacks.presence_updated = NULL;

	client_websocket_t *socket = createClient(&callbacks, "Mjg3MTc2MDM1MTUyMjk3OTg1.DD_c7w.V9NC_tbWiUZYv0jTEGTgyATLl6Q");

	// Test send message
	sleep(20);
	sendMessage("Hello, world!", 332535524869013505, 0);
	getMessagesInChannel(332535524869013505, 10);
	loadGuild(socket, 181866934353133570);

	// Keep the main thread occupied so the program doesn't exit
	while(1)
	{
		sleep(1);
	}

}

void onMessagePostedCallback(struct message message)
{
	printf("Recieved new message!\n");
}

void onReadyCallback(struct connection connection, struct server *servers)
{
	printf("Loaded guilds!\n");
	
	struct server *server = servers;
	
	while (server)
	{
		printf("- %s\n", server->name);
		server = server->next;
	}
}
