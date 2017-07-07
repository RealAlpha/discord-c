#include "websocket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

int client_ws_receive_callback(client_websocket_t *socket, char *data, size_t length);
int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length);

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
	printf("About to send request!\n");
	char *request =  "{\"op\":2,\"d\":{\"token\":\"Mjg3MTc2MDM1MTUyMjk3OTg1.DD_c7w.V9NC_tbWiUZYv0jTEGTgyATLl6Q\",\"properties\":{\"$os\":\"linux\",\"$browser\":\"my_library_name\",\"$device\":\"my_library_name\",\"$referrer\":\"\",\"$referring_domain\":\"\"},\"compress\":true,\"large_threshold\":250,\"shard\":[1,10]}}";

	websocket_send(myWebSocket, request, strlen(request), 0);
	//websocket_think(myWebSocket);
	//sleep(10);
	//websocket_disconnect(myWebSocket);
	//return -1;
	//else
	//	websocket_connect(myWebSocket, argv[1]);
	printf("Sent request!\n");
	while (1)
	{
		char *packet = "{\"op\": 1, \"d\": 251}";
		websocket_send(myWebSocket, packet, strlen(packet), 0);
		websocket_think(myWebSocket);
		usleep(41250*1000);
	}
}

int client_ws_receive_callback(client_websocket_t* socket, char* data, size_t length) {
	printf("\n\nrecieve callback!\nContent:\n%s\n\n", data);
	return 0;
}

int client_ws_connection_error_callback(client_websocket_t* socket, char* reason, size_t length) {
	//discord_client_t* client = (discord_client_t*)websocket_get_userdata(socket);

	printf("Connection error: %s (%zu)\n", reason, length);

	//client_disconnect(client);
	return 0;
}
