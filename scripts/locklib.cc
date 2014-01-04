// gcc -shared -fpic locklib.cc -o ../obj/scripts/locklib.so --llua5.1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>

#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lualib.h>

static int
acquire_lock(lua_State *const L)
{
    const char *const path = lua_tolstring(L, 1, NULL);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    // abstract socket
    strncpy(&addr.sun_path[1], path, sizeof(addr.sun_path) - 2);
    addr.sun_family = AF_UNIX;

    int sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        fprintf(stderr, "failed to open socket!\n");

        lua_pushboolean(L, false);
        lua_pushinteger(L, -1);
        return 2;
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        fprintf(stderr, "failed to bind socket!\n");

        lua_pushboolean(L, false);
        lua_pushinteger(L, -1);
        return 2;
    }

    lua_pushboolean(L, true);
    lua_pushinteger(L, sock);
    return 2;
}

static int
release_lock(lua_State *const L)
{
    const int sock = lua_tointeger(L, 1);
    close(sock);

    return 0;
}

static const struct luaL_reg locklib[] = {
    {"acquire_lock", acquire_lock},
    {"release_lock", release_lock},
    {NULL, NULL}
};

extern "C" int luaopen_locklib(lua_State *L);

int
luaopen_locklib(lua_State *L)
{
    luaL_openlib(L, "locklib", locklib, 0);
    return 1;
}
