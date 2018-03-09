/* 
 * Copyright (c) 2018 - 2019, CUJO LLC.
 * 
 * Licensed under the MIT license:
 * 
 *     http://www.opensource.org/licenses/mit-license.php
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>

#include <lua.h>
#include <lauxlib.h>

#define ARPTABLE_MT "larptab"

typedef struct larptab {
	int sock;
	struct arpreq req;
} larptab;

#define chklarp(L)	((larptab *)luaL_checkudata(L, 1, ARPTABLE_MT))

/* string = tostring(watcher) */
static int
larp_tostring (lua_State *L)
{
	larptab *larp = chklarp(L);

	if (larp->sock == -1)
		lua_pushliteral(L, "ARP (closed)");
	else
		lua_pushfstring(L, "ARP (%p)", larp);
	return 1;
}


static int
closearptab (lua_State *L, larptab *larp)
{
	int err = close(larp->sock);

	if (!err) larp->sock = -1;  /* mark watcher as closed */
	return luaL_fileresult(L, !err, NULL);
}

static int
larp_gc(lua_State *L)
{
	larptab *larp = chklarp(L);

	if (larp->sock != -1) closearptab(L, larp);
	return 0;
}


static larptab *
tolarp (lua_State *L) {
	larptab *larp = chklarp(L);

	if (larp->sock == -1) luaL_error(L, "attempt to use a closed ARP table");
	return larp;
}

/* succ [, errmsg] = watcher:close() */
static int
larp_close(lua_State *L)
{
	larptab *larp = tolarp(L);

	return closearptab(L, larp);
}

static int
larp_create(lua_State *L)
{
	size_t devlen;
	const char *devname = luaL_checklstring(L, 1, &devlen);
	larptab *larp = (larptab *)lua_newuserdata(L, sizeof(larptab));

	luaL_argcheck(L, devlen < sizeof(larp->req.arp_dev), 1,
		"interace device name too long");
	larp->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (larp->sock == -1) 	return luaL_fileresult(L, 0, NULL);
	memset(&larp->req, 0, sizeof(larp->req));
	larp->req.arp_pa.sa_family = AF_INET;
	larp->req.arp_ha.sa_family = ARPHRD_ETHER;
	memcpy(larp->req.arp_dev, devname, devlen);
	luaL_setmetatable(L, ARPTABLE_MT);
	return 1;
}

static int
getmac(larptab *larp)
{
	if (ioctl(larp->sock, SIOCGARP, &larp->req) != -1) {
		/* check if SIOCGARP gave us a bogus ARP of zeroes */
		struct ether_addr *macaddr = (struct ether_addr *)larp->req.arp_ha.sa_data;
		int i;

		for (i = 0; i < sizeof(macaddr->ether_addr_octet); ++i)
			if (macaddr->ether_addr_octet[i]) return 1;
	}
	return 0;
}

static int
sendudp(larptab *larp)
{
	int sent = sendto(larp->sock, NULL, 0, 0,
	                  &larp->req.arp_pa,
	                  sizeof(struct sockaddr_in));

	return sent != -1;
}

static int
larp_getmac(lua_State *L)
{
	larptab *larp = tolarp(L);
	const char *ip = luaL_checkstring(L, 2);
	struct sockaddr_in *inetaddr = (struct sockaddr_in *)&larp->req.arp_pa;

	luaL_argcheck(L, inet_aton(ip, &inetaddr->sin_addr), 2,
		"invalid IP address");
	if (getmac(larp) || (sendudp(larp) && getmac(larp)))
		lua_pushlstring(L, larp->req.arp_ha.sa_data, sizeof(struct ether_addr));
	else
		lua_pushnil(L);
	return 1;
}

static const luaL_Reg meta[] = {
	{"__gc", larp_gc},
	{"__tostring", larp_tostring},
	{NULL, NULL}
};

static const luaL_Reg meth[] = {
	{"close", larp_close},
	{"getmac", larp_getmac},
	{NULL, NULL}
};

static const luaL_Reg func[] = {
	{"create", larp_create},
	{NULL, NULL}
};

LUALIB_API int
luaopen_cujo_arptab (lua_State *L)
{
	luaL_newmetatable(L, ARPTABLE_MT);
	luaL_setfuncs(L, meta, 0);
	luaL_newlibtable(L, meth);
	luaL_setfuncs(L, meth, 0);
	lua_setfield(L, -2, "__index");
	luaL_newlibtable(L, func);
	luaL_setfuncs(L, func, 0);
	return 1;
}
