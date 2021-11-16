.PHONY: ALL

ALL:
# Set the library search path - users must copy this export
	export LD_LIBRARY_PATH=.:/usr/local/lib
# Comiple
	clang -ggdb3 -c websocket.c -o libdiscordws.o
	clang -ggdb3 -c discord-c.c -I/usr/local/include/cjson -o libdiscord-c.o
	ar rcs libdiscord-c.a -o libdiscordws.o libdiscord-c.o
	@echo Successfully built discord-c library.

clean:
# Remove the object files
	rm libdiscordws.o libdiscord-c.o
# Remove the library file
	rm libdiscord-c.a
