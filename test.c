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

struct rgbColor
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

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
	
	// Keep the main thread occupied so the program doesn't exit
	while(1)
	{
		char characters[2001];
		char lastCharacter = ' ';
		int len = 0;
		while (lastCharacter != '\n' && len <= 2000)
		{
			lastCharacter = getchar();
			characters[len] = lastCharacter;
			len++;
		}
		// Null-terminate the string
		characters[len] = '\0';
		if (strncmp(characters, "q", 1) == 0)
		{
			cleanup();
			return 0;
		}
		
		sendMessage(characters, 181866934353133570, 0);
	//	sleep(1);
	}

}

int hexToNum(char hex)
{
	char hexThing[2];
	hexThing[0] = hex;
	hexThing[1] = '\0';
	
	if (hex == '0')
		return 0;
	else if (hex == '1')
		return 1;
	else if (hex == '2')
		return 2;
	else if (hex == '3')
		return 3;
	else if (hex == '4')
		return 4;
	else if (hex == '5')
		return 5;
	else if (hex == '6')
		return 6;
	else if (hex == '7')
		return 7;
	else if (hex == '8')
		return 8;
	else if (hex == '9')
		return 9;
	//if (atoi(hexThing) < 10)
	//	return atoi(hexThing);
		//return 0;
	else if (hex == 'A')
		return 10;
	else if (hex == 'B')
		return 11;
	else if (hex == 'C')
		return 12;
	else if (hex == 'D')
		return 13;
	else if (hex == 'E')
		return 14;
	else if (hex == 'F')
		return 15;
	else
		return 0;
}

char *discordColorToHex(int color)
{
	char *array = malloc(7);
	int counter = 5; // 6 char long hex

	while(color!=0)
	{
		int result = color % 16;
		if (result < 10)
		{
			char s[5];
			sprintf(s, "%i", result);
			array[counter] = s[0];
		}
		else if (result == 10)
		{
			array[counter] = 'A';
		}
		else if (result == 11)
		{
			array[counter] = 'B';
		}
		else if (result == 12)
		{
			array[counter] = 'C';
		}
		else if (result == 13)
		{
			array[counter] = 'D';
		}
		else if (result == 14)
		{
			array[counter] = 'E';
		}
		else if (result == 15)
		{
			array[counter] = 'F';
		}
		else
		{
			array[counter] = ' ';
		}

		//array[counter]=number%16;
		color/=16;
		--counter;
		// *clamp* the value
		if (counter < 0)
			counter = 0;
	}	
	
	// Add a null-terminator to the end
	array[6] = '\0';
	return array;
}

struct rgbColor hexToRgb(char *hex)
{
	uint8_t	r = hexToNum(hex[0])*16+hexToNum(hex[1]);
	uint8_t g = hexToNum(hex[2])*16+hexToNum(hex[3]);
	uint8_t b = hexToNum(hex[4])*16+hexToNum(hex[5]);

	struct rgbColor color;

	color.r = r;
	color.g = g;
	color.b = b;

	return color;
}

void onMessagePostedCallback(struct message message)
{
	// Find the highest role the user has
	struct role *role = NULL;
	struct roles *role_trav = message.author->roles;

	while (role_trav)
	{
		if (role == NULL || role_trav->role->position > role->position)
			role = role_trav->role;

		role_trav = role_trav->next;
	}	

	// discord color -> rgb
	char *hex = discordColorToHex(role->color);
	struct rgbColor color = hexToRgb(hex);
	free(hex);
	
	// Print the author, channel & message in the author's role's color
	printf("\r\x1b[38;2;%i;%i;%im%s > %s > %s\x1b[0m\n", color.r, color.g, color.b, message.author->user->username, message.channel->name, message.body);

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

	// Load a guild
	client_websocket_t *socket = connection.webSocket;
	loadGuild(socket, 181866934353133570);
	//printf(">");	
}
