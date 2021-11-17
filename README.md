# Discord-C

*a C library for the Discord API*

Written by Alpha-V. 
Please open an issue if you're experiencing any issues/have feature suggestions!

**Please note that this is an old project**, and future updates are not guaranteed. 
So use at your own risk!

![Screenshot of example app](http://i.imgur.com/dLAEncM.png?1)

### Build from source

1. Clone Discord-C:

```sh
git clone https://github.com/RealAlpha/discord-c.git
cd discord-c
```

2. Set your token by going to main.c and updating `<YOUR TOKEN HERE>` with your own token. (obtained from LocalStorage or a bot token; keep the Discord TOS in mind).
3. Build Discord-C:

```sh
make
```

### Example Application

Run the Discord-C example:

```sh
./test
```

### Linking library as a dependency

The discord-c static library is `libdiscord-c.a`. 
To compile your Discord-C programs, please use the following gcc/clang flags to include Discord-C and its dependencies:

```sh
-I/path/to/discord-c/lib -L/path/to/discord-c/lib -lwebsockets -lssl -lcrypto -lpthread -lcjson -lcurl -ldiscord-c
```

### Dependencies

Discord-C depends on the following libraries:

- [clang](https://clang.llvm.org)
- [libwebsockets](https://libwebsockets.org)
- [libcurl](https://curl.se/libcurl/)
- [OpenSSL](https://github.com/openssl/openssl)
- [cJSON](https://github.com/DaveGamble/cJSON)

## Please check out disclird

It is a Discord CLI (WIP) written in C!

https://github.com/Audiatorix/Disclird
