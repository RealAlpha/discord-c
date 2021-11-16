.PHONY: ALL

ALL:
	@echo Please copy the following export if you are experiencing cJSON linking errors
# Set the library search path - users must copy this export
	export LD_LIBRARY_PATH=.:/usr/local/lib
# Build the library
	cd lib && $(MAKE) ALL
# Comiple
	clang -ggdb3 main.c  -I/usr/local/include/cjson -L/usr/local/lib -I./lib -L./lib -lwebsockets -lssl -lcrypto -lpthread -lcjson -lcurl -ldiscord-c -o test
	@echo Successfully built discord-c!

clean:
# Clean the library
	cd lib && $(MAKE) clean
# Remove the test executable
	rm test
