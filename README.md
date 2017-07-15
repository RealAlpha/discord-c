# Discord-C
A c discord API....that needs some work

# Build Discord-C
### Build the library & example application
Clone Discord-C
##### git clone https://github.com/RealAlpha/discord-c.git
##### cd discord-c
Set your token by going to main.c and updating <YOUR TOKEN HERE> with your own token. (obtained from LocalStorage).
Build Discord-C:
##### make
Run the Discord-C example:
##### ./test

### Build the library
Clone Discord-C
##### git clone https://github.com/RealAlpha/discord-c.git
##### cd discord-c/lib
Build Discord-C:
##### make
The discord-c static library is libdiscord-c.a. To compile your Discord-C programs please use the following gcc/clang flags:
#####  -I/path/to/discord-c/lib -L/path/to/discord-c/lib -lwebsockets -lssl -lcrypto -lpthread -lcjson -lcurl -ldiscord-c

# Please check out disclird, the (WIP) Discord CLI written in c!
https://github.com/Audiatorix/Disclird

Written by Alpha-V. Please open an issue if you're experiencing any issues/have feature suggestions!
