.PHONY: ALL

ALL:
# Set the library search path
	export LD_LIBRARY_PATH=.:/usr/local/lib
# Comiple
	clang -ggdb3 test.c discord-c.c websocket.c -I/usr/local/include/cjson -L/usr/local/lib -lwebsockets -lssl -lcrypto -lpthread -lcjson -lcurl -o test

