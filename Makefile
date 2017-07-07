.PHONY: ALL

ALL:
# Set the library search path
	export LD_LIBRARY_PATH=.
# Comiple
	clang -ggdb3 discord-c.c websocket.c -lwebsockets -lssl -lcrypto -lpthread -o test

