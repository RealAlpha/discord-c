.PHONY: ALL

ALL:
	clang -ggdb3 discord-c.c websocket.c -lwebsockets -lssl -lcrypto -o test

