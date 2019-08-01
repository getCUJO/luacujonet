--
-- Copyright (C) 2015-2019  CUJO LLC
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License along
-- with this program; if not, write to the Free Software Foundation, Inc.,
-- 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
--

local attribs = {
	"up",
	"broadcast",
	"debug",
	"loopback",
	"pointopoint",
	"running",
	"noarp",
	"promisc",
	"notrailers",
	"allmulti",
	"master",
	"slave",
	"multicast",
	"portsel",
	"automedia",
	"dynamic",
}
local function getattribs(...)
	if ... == nil then return ... end
	local list = {}
	for i = 1, select("#", ...) do
		if select(i, ...) then
			list[#list+1] = attribs[i]
		end
	end
	return table.concat(list, ",")
end

local function readfrom(path)
	local file = assert(io.open(path))
	local contents = file:read()
	file:close()
	return contents
end

local net = require "rabid.net"

local netcfg = assert(net.newcfg())

for i = 1, math.huge do
	local name = netcfg:getdevname(i)
	if name == nil then break end
	print("#", i)
	print("  Device Name", name)
	print("  Device Address", netcfg:getdevaddr(name))
	print("  Destination Address", netcfg:getdevdstaddr(name))
	print("  Broadcast Address", netcfg:getdevbroadaddr(name))
	print("  Hardware Address", netcfg:getdevhwaddr(name))
	print("  Netowk Submask", netcfg:getdevnetmask(name))
	print("  Maximum Transfer Unit", netcfg:getdevmtu(name))
	print("  Extra Attributes", getattribs(netcfg:getdevattrib(name, table.unpack(attribs))))
	print("  Gateway MAC", netcfg:getarpentry(name, "10.212.0.1"))
end


assert(netcfg:close())
