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
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>

#include <lua.h>
#include <lauxlib.h>

#define CUJO_NETCFGMT "cujo_LuaNetCfg"

typedef struct LuaNetCfg {
	int sock;
} LuaNetCfg;

#define chklnetcfg(L)	((LuaNetCfg *)luaL_checkudata(L, 1, CUJO_NETCFGMT))

/* string = tostring(watcher) */
static int lcfg_tostring (lua_State *L)
{
	LuaNetCfg *lnetcfg = chklnetcfg(L);
	if (lnetcfg->sock == -1)
		lua_pushliteral(L, "netcfg (closed)");
	else
		lua_pushfstring(L, "netcfg (%p)", lnetcfg);
	return 1;
}


static int closenetcfg (lua_State *L, LuaNetCfg *lnetcfg)
{
	int err = close(lnetcfg->sock);
	if (!err) lnetcfg->sock = -1;  /* mark watcher as closed */
	return luaL_fileresult(L, !err, NULL);
}

static int lcfg_gc(lua_State *L)
{
	LuaNetCfg *lnetcfg = chklnetcfg(L);
	if (lnetcfg->sock != -1) closenetcfg(L, lnetcfg);
	return 0;
}


static LuaNetCfg *tolnetcfg (lua_State *L) {
	LuaNetCfg *lnetcfg = chklnetcfg(L);
	if (lnetcfg->sock == -1) luaL_error(L, "attempt to use a closed ARP table");
	return lnetcfg;
}

/* succ [, errmsg] = watcher:close() */
static int lcfg_close(lua_State *L)
{
	LuaNetCfg *lnetcfg = tolnetcfg(L);
	return closenetcfg(L, lnetcfg);
}

static int lnet_newcfg(lua_State *L)
{
	LuaNetCfg *lnetcfg = (LuaNetCfg *)lua_newuserdata(L, sizeof(LuaNetCfg));
	lnetcfg->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (lnetcfg->sock == -1) return luaL_fileresult(L, 0, NULL);
	luaL_setmetatable(L, CUJO_NETCFGMT);
	return 1;
}

#define MAX_LITERAL_ADDR (INET6_ADDRSTRLEN+1)
static void pushaddr(lua_State *L, struct sockaddr *addr)
{
	char buf[MAX_LITERAL_ADDR];
	switch (addr->sa_family) {
		case AF_INET: {
			struct sockaddr_in *inet = (struct sockaddr_in *)addr;
			lua_pushstring(L, inet_ntop(AF_INET, &inet->sin_addr.s_addr,
			                                     buf, MAX_LITERAL_ADDR));
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *inet6 = (struct sockaddr_in6 *)addr;
			lua_pushstring(L, inet_ntop(AF_INET6, &inet6->sin6_addr,
			                                      buf, MAX_LITERAL_ADDR));
		} break;
		case ARPHRD_ETHER: {
			struct ether_addr *hwaddr = (struct ether_addr *)addr->sa_data;
			lua_pushstring(L, ether_ntoa(hwaddr));
		} break;
		default: lua_pushstring(L, "<unknown address>");
	}
}

static int getmac(LuaNetCfg *lnetcfg, struct arpreq *req)
{
	if (ioctl(lnetcfg->sock, SIOCGARP, req) != -1) {
		struct ether_addr *addr = (struct ether_addr *)req->arp_ha.sa_data;
		int i;
		/* check if SIOCGARP gave us a bogus MAC of zeroes */
		for (i = 0; i < sizeof(addr->ether_addr_octet); ++i)
			if (addr->ether_addr_octet[i]) return 1;
	}
	return 0;
}

static int sendudp(LuaNetCfg *lnetcfg, struct arpreq *req)
{
	int sent = sendto(lnetcfg->sock, NULL, 0, 0, &req->arp_pa,
	                  sizeof(struct sockaddr_in));
	return sent != -1;
}

static int lcfg_getarpentry(lua_State *L)
{
	LuaNetCfg *lnetcfg = tolnetcfg(L);
	struct arpreq req;
	size_t devlen;
	const char *devname = luaL_checklstring(L, 2, &devlen);
	const char *ip = luaL_checkstring(L, 3);
	struct sockaddr_in *inetaddr = (struct sockaddr_in *)&req.arp_pa;
	luaL_argcheck(L, devlen < sizeof(req.arp_dev), 2,
		"interface device name too long");
	memset(&req, 0, sizeof(req));
	luaL_argcheck(L, inet_aton(ip, &inetaddr->sin_addr), 3,
		"invalid IP address");
	req.arp_pa.sa_family = AF_INET;
	req.arp_ha.sa_family = ARPHRD_ETHER;
	memcpy(req.arp_dev, devname, devlen);
	if (getmac(lnetcfg, &req) || (sendudp(lnetcfg, &req) && getmac(lnetcfg, &req)))
		pushaddr(L, &req.arp_ha);
	else
		lua_pushnil(L);
	return 1;
}

static int failcfg(lua_State *L, unsigned long action, struct ifreq *req)
{
	LuaNetCfg *lnetcfg = tolnetcfg(L);
	size_t devlen;
	const char *devname = luaL_checklstring(L, 2, &devlen);
	luaL_argcheck(L, devlen < IFNAMSIZ, 2, "interface device name too long");
	strncpy(req->ifr_name, devname, devlen);
	return ioctl(lnetcfg->sock, action, req);
}

#define lcfg_getparam(action, field, pushf) \
	static int lcfg_getdev##field(lua_State *L) \
	{ \
		struct ifreq req; \
		memset(&req, 0, sizeof(req)); \
		if (failcfg(L, action, &req)) return luaL_fileresult(L, 0, NULL); \
		pushf(L, &req.ifr_##field); \
		return 1; \
	}

#define lcfg_setparam(action, field, getargf) \
	static int lcfg_setdev##field(lua_State *L) \
	{ \
		struct ifreq req; \
		memset(&req, 0, sizeof(req)); \
		getargf(L, 3, &req.ifr_##field); \
		return luaL_fileresult(L, !failcfg(L, action, &req), NULL); \
	}

static void getinetaddr(lua_State *L, int arg, struct sockaddr *addr)
{
	const char *literal = luaL_checkstring(L, arg);
	struct sockaddr_in *inet = (struct sockaddr_in *)addr;
	int err = inet_pton(AF_INET, literal, &inet->sin_addr.s_addr);
	luaL_argcheck(L, err == 1, arg, "invalid IP address");
	inet->sin_family = AF_INET;
}

static void getetheraddr(lua_State *L, int arg, struct sockaddr *addr)
{
	const char *literal = luaL_checkstring(L, arg);
	struct ether_addr *hwaddr = (struct ether_addr *)addr->sa_data;
	struct ether_addr *res = ether_aton(literal);
	luaL_argcheck(L, res != NULL, arg, "invalid IP address");
	memcpy(hwaddr, res, sizeof(struct ether_addr));
}

static void pushintval(lua_State *L, int *value)
{
	lua_pushinteger(L, *value);
}

static void getintarg(lua_State *L, int arg, int *value)
{
	*value = (int)luaL_checkinteger(L, arg);
}

static int
checkattribflag(lua_State *L, int arg)
{
	static const struct { int flag; const char *name; } attrib[] = {
		{ IFF_UP, "up" },
		{ IFF_BROADCAST, "broadcast" },
		{ IFF_DEBUG, "debug" },
		{ IFF_LOOPBACK, "loopback" },
		{ IFF_POINTOPOINT, "pointopoint" },
		{ IFF_RUNNING, "running" },
		{ IFF_NOARP, "noarp" },
		{ IFF_PROMISC, "promisc" },
		{ IFF_NOTRAILERS, "notrailers" },
		{ IFF_ALLMULTI, "allmulti" },
		{ IFF_MASTER, "master" },
		{ IFF_SLAVE, "slave" },
		{ IFF_MULTICAST, "multicast" },
		{ IFF_PORTSEL, "portsel" },
		{ IFF_AUTOMEDIA, "automedia" },
		{ IFF_DYNAMIC, "dynamic" },
		{ 0, NULL }
	};
	const char *name = luaL_checkstring(L, arg);
	int i;
	for (i = 0; attrib[i].name; i++)
		if (strcmp(attrib[i].name, name) == 0)
			return attrib[i].flag;
	return luaL_argerror(L, arg, lua_pushfstring(L, "invalid flag '%s'", name));
}

static int lcfg_getdevattrib(lua_State *L)
{
	struct ifreq req;
	int arg, narg = lua_gettop(L);
	memset(&req, 0, sizeof(req));
	if (failcfg(L, SIOCGIFFLAGS, &req)) return luaL_fileresult(L, 0, NULL);
	for (arg = 3; arg <= narg; ++arg) {
		int flag = checkattribflag(L, arg);
		lua_pushboolean(L, req.ifr_flags & flag);
	}
	return narg-2;
}

static int lcfg_setdevattrib(lua_State *L)
{
	struct ifreq req;
	int arg, narg = lua_gettop(L);
	memset(&req, 0, sizeof(req));
	if (failcfg(L, SIOCGIFFLAGS, &req)) return luaL_fileresult(L, 0, NULL);
	for (arg = 3; arg <= narg-1; arg += 2) {
		int flag = checkattribflag(L, arg);
		int value = lua_toboolean(L, arg+1);
		if (value) req.ifr_flags |= flag;
		else req.ifr_flags &= ~flag;
	}
	return luaL_fileresult(L, !failcfg(L, SIOCSIFFLAGS, &req), NULL);
}

static int lcfg_getdevname(lua_State *L)
{
	struct ifreq req;
	LuaNetCfg *lnetcfg = tolnetcfg(L);
	lua_Integer index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, 0 <= index && index <= INT_MAX, 2, "invalid device index");
	memset(&req, 0, sizeof(req));
	req.ifr_ifindex = (int)index;
	if (ioctl(lnetcfg->sock, SIOCGIFNAME, &req))
		return luaL_fileresult(L, 0, NULL);
	lua_pushstring(L, req.ifr_name);
	return 1;
}

lcfg_getparam(SIOCGIFINDEX, ifindex, pushintval);
lcfg_getparam(SIOCGIFADDR, addr, pushaddr);
lcfg_setparam(SIOCSIFADDR, addr, getinetaddr);
lcfg_getparam(SIOCGIFDSTADDR, dstaddr, pushaddr);
lcfg_setparam(SIOCSIFDSTADDR, dstaddr, getinetaddr);
lcfg_getparam(SIOCGIFBRDADDR, broadaddr, pushaddr);
lcfg_setparam(SIOCSIFBRDADDR, broadaddr, getinetaddr);
lcfg_getparam(SIOCGIFNETMASK, netmask, pushaddr);
lcfg_setparam(SIOCSIFNETMASK, netmask, getinetaddr);
lcfg_getparam(SIOCGIFHWADDR, hwaddr, pushaddr);
lcfg_setparam(SIOCSIFHWADDR, hwaddr, getetheraddr);
lcfg_getparam(SIOCGIFMTU, mtu, pushintval);
lcfg_setparam(SIOCSIFMTU, mtu, getintarg);

static const luaL_Reg meta[] = {
	{"__gc", lcfg_gc},
	{"__tostring", lcfg_tostring},
	{NULL, NULL}
};

static const luaL_Reg meth[] = {
	{"close", lcfg_close},
	{"getarpentry", lcfg_getarpentry},
	{"getdevname", lcfg_getdevname},
	{"getdevindex", lcfg_getdevifindex},
	{"getdevattrib", lcfg_getdevattrib},
	{"setdevattrib", lcfg_setdevattrib},
	{"getdevaddr", lcfg_getdevaddr},
	{"setdevaddr", lcfg_setdevaddr},
	{"getdevdstaddr", lcfg_getdevdstaddr},
	{"setdevdstaddr", lcfg_setdevdstaddr},
	{"getdevbroadaddr", lcfg_getdevbroadaddr},
	{"setdevbroadaddr", lcfg_setdevbroadaddr},
	{"getdevhwaddr", lcfg_getdevhwaddr},
	{"setdevhwaddr", lcfg_setdevhwaddr},
	{"getdevnetmask", lcfg_getdevnetmask},
	{"setdevnetmask", lcfg_setdevnetmask},
	{"getdevmtu", lcfg_getdevmtu},
	{"setdevmtu", lcfg_setdevmtu},
	{NULL, NULL}
};

static const luaL_Reg func[] = {
	{"newcfg", lnet_newcfg},
	{NULL, NULL}
};

LUALIB_API int luaopen_cujo_net (lua_State *L)
{
	luaL_newmetatable(L, CUJO_NETCFGMT);
	luaL_setfuncs(L, meta, 0);
	luaL_newlibtable(L, meth);
	luaL_setfuncs(L, meth, 0);
	lua_setfield(L, -2, "__index");
	luaL_newlibtable(L, func);
	luaL_setfuncs(L, func, 0);
	return 1;
}