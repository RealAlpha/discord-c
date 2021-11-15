# Discord-C
A c discord API....that needs some work. **Please note that this is an old project**, and should not be used for anything important. Future updates are not guaranteed, so use at your own risk!

![Screenshot of example app](http://i.imgur.com/dLAEncM.png?1)

# Build Discord-C
### Build the library & example application
1. Clone Discord-C
```sh
git clone https://github.com/RealAlpha/discord-c.git
cd discord-c
```

2. Set your token by going to main.c and updating `<YOUR TOKEN HERE>` with your own token. (obtained from LocalStorage or a bot token; keep the Discord TOS in mind).

3. Build Discord-C:
```sh
make
```

4. Run the Discord-C example:
```sh
./test
```

### Build the library
1. Clone Discord-C
```sh
git clone https://github.com/RealAlpha/discord-c.git
cd discord-c/lib
```

2. Build Discord-C:
```sh
make
```
  
3. The discord-c static library is libdiscord-c.a. To compile your Discord-C programs please use the following gcc/clang flags to include Discord-C and its dependencies:
```sh
-I/path/to/discord-c/lib -L/path/to/discord-c/lib -lwebsockets -lssl -lcrypto -lpthread -lcjson -lcurl -ldiscord-c
```

# Dependencies
Discord-C depends on the following libraries:
- [libwebsockets](https://libwebsockets.org)
- [libcurl](https://curl.se/libcurl/)
- ([OpenSSL](https://github.com/openssl/openssl))
- [cJSON](https://github.com/DaveGamble/cJSON)

Furthermore, to use the built-in make files and examples, you'll need to use [clang](https://clang.llvm.org)

# Please check out disclird, the (WIP) Discord CLI written in c!
https://github.com/Audiatorix/Disclird

Written by Alpha-V. Please open an issue if you're experiencing any issues/have feature suggestions!
