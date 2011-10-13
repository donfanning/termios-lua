/*
Copyright (c) 2011 Wurldtech Security Technologies All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/

/*-
** termios - get and set terminal attributes, line control, and baud rate

These functions take either file descriptor numbers, or io objects from the
standard library as arguments.


Notes:

- Haven't tested portability, might need to test _BSD_SOURCE to see if cfsetspeed() or cfmakeraw() exist
- Error strings could be annotated with info about the system call and args that failed
- Could make better effort to document the functions, but the man pages really are the final reference.

*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


#define REGID "wt.termios"

static int push_error(lua_State* L)
{
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    lua_pushinteger(L, errno);

    return 3;
}

/* for a closed io object, errors with same emsg as lua's io library */
static int check_fileno(lua_State *L, int index)
{
    if (lua_isnumber(L, index)) {
        return luaL_checkint(L, index);
    } else {
        FILE** f = luaL_checkudata(L, index, LUA_FILEHANDLE);

        if (*f == NULL)
            luaL_error(L, "attempt to use a closed file");

        return fileno(*f);
    }
}

static int check_when(lua_State *L, int index)
{
    static const char* opts[] = { "now", "drain", "flush", NULL };
    static int opti[] = { TCSANOW, TCSADRAIN, TCSAFLUSH };

    return opti[ luaL_checkoption(L, index, "flush", opts) ];
}

#define check_tcgetattr(L, fd, termios) \
    if (tcgetattr(fd, termios) < 0) { \
        return push_error(L); \
    }

#define check_tcsetattr(L, fd, opt, termios) \
    if (tcsetattr(fd, opt, termios) < 0) { \
        return push_error(L); \
    }

/*-
-- fd = termios.fileno(io)

Return the file descriptor number for the specified io object.

Returns io on success, nil, errmsg, errno on failure.
*/
static int ltermios_fileno(lua_State *L)
{
    int fd = check_fileno(L, 1);

    lua_pushnumber(L, fd);

    return 1;
}

/*-
-- io = termios.setblocking(io[, blocking])

Blocking is true to set blocking, and false to set non-blocking (default is false)

Returns io on success, nil, errmsg, errno on failure.
*/
static int ltermios_setblocking(lua_State *L)
{
    int fd = check_fileno(L, 1);
    int block = lua_toboolean(L, 2);
    
    int flags = fcntl(fd, F_GETFL);
    if (flags<0) {
        return push_error(L);
    }
    
    /* to SET blocking, we CLEAR O_NONBLOCK */
    if (block) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }
    
    flags = fcntl(fd, F_SETFL, flags);
    if (flags<0) {
        return push_error(L);
    }
    
    lua_settop(L, 1);

    return 1;
}

/*-
-- io = termios.setcanonical(io, canonical, when)

Turns canonical mode on and off for a TTY.  Canonical defaults to true.

When is "now", "drain", or "flush". Default is "flush".

Returns io on success, nil, errmsg, errno on failure.
*/
/* TODO should canonical default to false? */
static int optboolean(lua_State* L, int narg, int d)
{
    if (lua_isnil(L, narg)) {
        return d;
    }
    return lua_toboolean(L, narg);
}
static int ltermios_setcanonical(lua_State *L)
{
    int fd = check_fileno(L, 1);
    int canonical = optboolean(L, 2, 1);
    int opt = check_when(L, 3);
    
    struct termios termios={0};

    check_tcgetattr(L, fd, &termios);

    if (canonical) {
        termios.c_lflag |= ICANON;
    }
    else {
        termios.c_lflag &= ~ICANON;
    }

    check_tcsetattr(L, fd, opt, &termios);

    lua_settop(L, 1);

    return 1;
}

/*-
-- io = termios.cfsetspeed(io, speed, when)
-- io = termios.cfsetispeed(io, speed, when)
-- io = termios.cfsetospeed(io, speed, when)

Set speed for input and output, input only, or output only.

Speed is the baud rate, and must be one of those supported by termios, 0, 1200,
1400, 4800, 9600, 38400 are common.

For output, a speed of zero disconnects the line.

For input, a speed of zero means set the input speed to the output speed.

When is "now", "drain", or "flush". Default is "flush".

Returns io on success, nil, errmsg, errno on failure.
*/

static struct {
    speed_t speed;
    int baud;
} speeds[] = {
#ifdef B0
    { B0, 0 },
#endif
#ifdef B50
    { B50, 50 },
#endif
#ifdef B75
    { B75, 75 },
#endif
#ifdef B110
    { B110, 110 },
#endif
#ifdef B134
    { B134, 134 },
#endif
#ifdef B150
    { B150, 150 },
#endif
#ifdef B200
    { B200, 200 },
#endif
#ifdef B300
    { B300, 300 },
#endif
#ifdef B600
    { B600, 600 },
#endif
#ifdef B1200
    { B1200, 1200 },
#endif
#ifdef B1800
    { B1800, 1800 },
#endif
#ifdef B2400
    { B2400, 2400 },
#endif
#ifdef B4800
    { B4800, 4800 },
#endif
#ifdef B9600
    { B9600, 9600 },
#endif
#ifdef B19200
    { B19200, 19200 },
#endif
#ifdef B38400
    { B38400, 38400 },
#endif
#ifdef B57600
    { B57600, 57600 },
#endif
#ifdef B115200
    { B115200, 115200 },
#endif
#ifdef B230400
    { B230400, 230400 },
#endif
};
static int SPEEDS = sizeof(speeds)/sizeof(speeds[0]);

typedef int cfspeedfn(struct termios *termios_p, speed_t speed);

static int setspeed(lua_State* L, cfspeedfn* speedfn)
{
    int fd = check_fileno(L, 1);
    int baud = luaL_checkint(L, 2);
    int opt = check_when(L, 3);
    speed_t speed = 0;
    struct termios termios;
    int i;

    for (i = 0; i < SPEEDS; i++) {
        if (speeds[i].baud == baud) {
            speed = speeds[i].speed;
            break;
        }
    }

    if (i == SPEEDS) {
        lua_pushnil(L);
        lua_pushstring(L, "unsupported speed");
        lua_pushnumber(L, EINVAL);
        return 3;
    }

    check_tcgetattr(L, fd, &termios);

    if (speedfn(&termios, speed) < 0) {
        return push_error(L);
    }

    check_tcsetattr(L, fd, opt, &termios);

    lua_settop(L, 1);

    return 1;
}

static int ltermios_cfsetspeed(lua_State* L)
{
    return setspeed(L, cfsetspeed);
}
static int ltermios_cfsetispeed(lua_State* L)
{
    return setspeed(L, cfsetispeed);
}
static int ltermios_cfsetospeed(lua_State* L)
{
    return setspeed(L, cfsetospeed);
}

/*-
-- io = termios.tcflush(io, direction)

See man page for tcflush()

Direction is either "in", "out", or "both". It defaults to "both".

Returns io on success, nil, errmsg, errno on failure.
*/
static int ltermios_tcflush(lua_State *L)
{
    int fd = check_fileno(L, 1);

    static const char* opts[] = { "in", "out", "both", NULL };
    static int opti[] = { TCIFLUSH, TCOFLUSH, TCIOFLUSH };

    int opt = opti[ luaL_checkoption(L, 2, "both", opts) ];

    if(tcflush(fd, opt) < 0) {
        return push_error(L);
    }

    lua_settop(L, 1);

    return 1;
}

/*-
-- io = termios.tcdrain(io).

See man page for tcdrain()

Returns io on success, nil, errmsg, errno on failure.
*/
static int ltermios_tcdrain(lua_State *L)
{
    int fd = check_fileno(L, 1);

    if(tcdrain(fd) < 0) {
        return push_error(L);
    }

    lua_settop(L, 1);

    return 1;
}

/*-
-- io = termios.tcsendbreak(io, duration).

See man page for tcsendbreak().

Duration is optional, and defaults to zero. If non-zero, its meaning is
apparently implementation-defined, it might even be ignored.

Returns io on success, nil, errmsg, errno on failure.
*/
static int ltermios_tcsendbreak(lua_State *L)
{
    int fd = check_fileno(L, 1);
    int duration = luaL_optint(L, 2, 0);

    if(tcsendbreak(fd, duration) < 0) {
        return push_error(L);
    }

    lua_settop(L, 1);

    return 1;
}

/*-
-- io = termios.cfraw(io, when)

See man page for cfmakeraw()

When is "now", "drain", or "flush". Default is "flush".

Returns io on success, nil, errmsg, errno on failure.
*/
static int ltermios_cfraw(lua_State *L)
{
    int fd = check_fileno(L, 1);
    int opt = check_when(L, 2);
    struct termios termios;

    check_tcgetattr(L, fd, &termios);

    cfmakeraw(&termios);

    check_tcsetattr(L, fd, opt, &termios);

    lua_settop(L, 1);

    return 1;
}

/*-
-- fd = termios.open(path)

The path must exist, and is opened read-write.

Returns fd on success, nil, errmsg, errno on failure.
*/
/* Could I depend on luaposix for this? */
static int ltermios_open(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int fd = open(path, O_NOCTTY|O_RDWR);

    if (fd < 0) {
        return push_error(L);
    }

    lua_pushinteger(L, fd);

    return 1;
}

/*-
-- termios.close(fd)

Close an fd, which must be a number, not an io object.

Returns nothing on success, nil, errmsg, errno on failure.
*/
static int ltermios_close(lua_State *L)
{
    if (close(luaL_checkint(L, 1)) < 0) {
            return push_error(L);
    }

    return 0;
}

/*
Missing termios functions TBD:
- wrapping struct termios... quite the task
- cfgetispeed()
- cfgetospeed()
*/
static const luaL_reg termios[] =
{
    {"fileno",            ltermios_fileno},
    {"setblocking",       ltermios_setblocking},
    {"setcanonical",      ltermios_setcanonical},
    {"tcflush",           ltermios_tcflush},
    {"tcdrain",           ltermios_tcdrain},
    {"tcsendbreak",       ltermios_tcsendbreak},
    {"cfraw",             ltermios_cfraw},
    {"cfsetspeed",        ltermios_cfsetspeed},
    {"cfsetispeed",       ltermios_cfsetispeed},
    {"cfsetospeed",       ltermios_cfsetospeed},
    {"open",              ltermios_open},
    {"close",             ltermios_close},
    {NULL, NULL}
};

LUALIB_API int luaopen_termios (lua_State *L)
{
    luaL_register(L, "termios", termios);

    return 1;
}

