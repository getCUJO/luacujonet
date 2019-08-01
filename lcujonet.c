/*
 * Copyright (C) 2015-2019  CUJO LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
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

#define RABID_NETCFGMT "lnetconf"

typedef struct lnetconf {
	int sock;
} lnetconf;

#define chklnetconf(L)	((lnetconf *)luaL_checkudata(L, 1, RABID_NETCFGMT))

/* string = tostring(watcher) */
static int
lcfg_tostring (lua_State *L)
{
	lnetconf *conf = chklnetconf(L);
	if (conf->sock == -1)
		lua_pushliteral(L, "netcfg (closed)");
	else
		lua_pushfstring(L, "netcfg (%p)", conf);
	return 1;
}


static int
closenetcfg (lua_State *L, lnetconf *conf)
{
	int err = close(conf->sock);
	if (!err) conf->sock = -1;  /* mark watcher as closed */
	return luaL_fileresult(L, !err, NULL);
}

static int
lcfg_gc(lua_State *L)
{
	lnetconf *conf = chklnetconf(L);
	if (conf->sock != -1) closenetcfg(L, conf);
	return 0;
}


static lnetconf *
tolnetconf (lua_State *L) {
	lnetconf *conf = chklnetconf(L);
	if (conf->sock == -1) luaL_error(L, "attempt to use a closed ARP table");
	return conf;
}

/* succ [, errmsg] = watcher:close() */
static int
lcfg_close(lua_State *L)
{
	lnetconf *conf = tolnetconf(L);
	return closenetcfg(L, conf);
}

static int
lnet_newcfg(lua_State *L)
{
	lnetconf *conf = (lnetconf *)lua_newuserdata(L, sizeof(lnetconf));
	conf->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (conf->sock == -1) return luaL_fileresult(L, 0, NULL);
	luaL_setmetatable(L, RABID_NETCFGMT);
	return 1;
}

#define MAX_LITERAL_ADDR (INET6_ADDRSTRLEN+1)
static void
pushaddr(lua_State *L, struct sockaddr *addr)
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

static int
getmac(lnetconf *conf, struct arpreq *req)
{
	if (ioctl(conf->sock, SIOCGARP, req) != -1) {
		struct ether_addr *addr = (struct ether_addr *)req->arp_ha.sa_data;
		int i;
		/* check if SIOCGARP gave us a bogus MAC of zeroes */
		for (i = 0; i < sizeof(addr->ether_addr_octet); ++i)
			if (addr->ether_addr_octet[i]) return 1;
	}
	return 0;
}

static size_t addr_size[] = {sizeof(struct in_addr), sizeof(struct in6_addr)};
static int addr_domain[] = {AF_INET, AF_INET6};
static const char *const addr_opts[] = {"ipv4", "ipv6", NULL};

static int
lnet_iptobin(lua_State *L)
{
	unsigned char buf[sizeof(struct in6_addr)];
	int kind = luaL_checkoption(L, 1, NULL, addr_opts);
	const char *addr = luaL_checkstring(L, 2);
	if (inet_pton(addr_domain[kind], addr, buf) == 1) {
		lua_pushlstring(L, buf, addr_size[kind]);
		return 1;
	}
	lua_pushnil(L);
	lua_pushstring(L, strerror(errno));
	return 2;
}

static int
lnet_bintoip(lua_State *L)
{
	static size_t maxsz[] = {INET_ADDRSTRLEN, INET6_ADDRSTRLEN};
	unsigned char buf[INET6_ADDRSTRLEN];
	int kind = luaL_checkoption(L, 1, NULL, addr_opts);
	size_t n;
	const char *addr = luaL_checklstring(L, 2, &n);
	luaL_argcheck(L, n == addr_size[kind], 2, "invalid address");
	if (inet_ntop(addr_domain[kind], addr, buf, maxsz[kind]) != NULL) {
		lua_pushstring(L, buf);
		return 1;
	}
	lua_pushnil(L);
	lua_pushstring(L, strerror(errno));
	return 2;
}

static int
sendudp(lnetconf *conf, struct arpreq *req)
{
	int sent = sendto(conf->sock, NULL, 0, 0, &req->arp_pa,
	                  sizeof(struct sockaddr_in));
	return sent != -1;
}

static int
lcfg_getarpentry(lua_State *L)
{
	lnetconf *conf = tolnetconf(L);
	struct arpreq req;
	size_t devlen;
	const char *devname = luaL_checklstring(L, 2, &devlen);
	const char *ip = luaL_checkstring(L, 3);
	int cacheonly = lua_toboolean(L, 4);
	struct sockaddr_in *inetaddr = (struct sockaddr_in *)&req.arp_pa;
	luaL_argcheck(L, devlen < sizeof(req.arp_dev), 2,
		"interface device name too long");
	memset(&req, 0, sizeof(req));
	luaL_argcheck(L, inet_aton(ip, &inetaddr->sin_addr), 3,
		"invalid IP address");
	req.arp_pa.sa_family = AF_INET;
	req.arp_ha.sa_family = ARPHRD_ETHER;
	memcpy(req.arp_dev, devname, devlen);
	if (getmac(conf, &req) || (!cacheonly &&
	                           sendudp(conf, &req) &&
	                           getmac(conf, &req)))
		pushaddr(L, &req.arp_ha);
	else
		lua_pushnil(L);
	return 1;
}

static int
failcfg(lua_State *L, unsigned long action, struct ifreq *req)
{
	lnetconf *conf = tolnetconf(L);
	size_t devlen;
	const char *devname = luaL_checklstring(L, 2, &devlen);
	luaL_argcheck(L, devlen < IFNAMSIZ, 2, "interface device name too long");
	strncpy(req->ifr_name, devname, devlen);
	return ioctl(conf->sock, action, req);
}

#define lcfg_getparam(action, field, pushf) \
	static int \
	lcfg_getdev##field(lua_State *L) \
	{ \
		struct ifreq req; \
		memset(&req, 0, sizeof(req)); \
		if (failcfg(L, action, &req)) return luaL_fileresult(L, 0, NULL); \
		pushf(L, &req.ifr_##field); \
		return 1; \
	}

#define lcfg_setparam(action, field, getargf) \
	static int \
	lcfg_setdev##field(lua_State *L) \
	{ \
		struct ifreq req; \
		memset(&req, 0, sizeof(req)); \
		getargf(L, 3, &req.ifr_##field); \
		return luaL_fileresult(L, !failcfg(L, action, &req), NULL); \
	}

static void
getinetaddr(lua_State *L, int arg, struct sockaddr *addr)
{
	const char *literal = luaL_checkstring(L, arg);
	struct sockaddr_in *inet = (struct sockaddr_in *)addr;
	int err = inet_pton(AF_INET, literal, &inet->sin_addr.s_addr);
	luaL_argcheck(L, err == 1, arg, "invalid IP address");
	inet->sin_family = AF_INET;
}

static void
getetheraddr(lua_State *L, int arg, struct sockaddr *addr)
{
	const char *literal = luaL_checkstring(L, arg);
	struct ether_addr *hwaddr = (struct ether_addr *)addr->sa_data;
	struct ether_addr *res = ether_aton(literal);
	luaL_argcheck(L, res != NULL, arg, "invalid IP address");
	memcpy(hwaddr, res, sizeof(struct ether_addr));
}

static void
pushintval(lua_State *L, int *value)
{
	lua_pushinteger(L, *value);
}

static void
getintarg(lua_State *L, int arg, int *value)
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

static int
lcfg_getdevattrib(lua_State *L)
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

static int
lcfg_setdevattrib(lua_State *L)
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

static int
lcfg_getdevname(lua_State *L)
{
	struct ifreq req;
	lnetconf *conf = tolnetconf(L);
	lua_Integer index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, 0 <= index && index <= INT_MAX, 2, "invalid device index");
	memset(&req, 0, sizeof(req));
	req.ifr_ifindex = (int)index;
	if (ioctl(conf->sock, SIOCGIFNAME, &req))
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
	{"iptobin", lnet_iptobin},
	{"bintoip", lnet_bintoip},
	{NULL, NULL}
};

LUALIB_API int
luaopen_rabid_net (lua_State *L)
{
	luaL_newmetatable(L, RABID_NETCFGMT);
	luaL_setfuncs(L, meta, 0);
	luaL_newlibtable(L, meth);
	luaL_setfuncs(L, meth, 0);
	lua_setfield(L, -2, "__index");
	luaL_newlibtable(L, func);
	luaL_setfuncs(L, func, 0);
	return 1;
}
