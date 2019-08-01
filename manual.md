_Copyright (C) 2015-2019  CUJO LLC_

_This program is free software; you can redistribute it and/or modify_
_it under the terms of the GNU General Public License as published by_
_the Free Software Foundation; either version 2 of the License, or_
_(at your option) any later version._

_This program is distributed in the hope that it will be useful,_
_but WITHOUT ANY WARRANTY; without even the implied warranty of_
_MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the_
_GNU General Public License for more details._

_You should have received a copy of the GNU General Public License along_
_with this program; if not, write to the Free Software Foundation, Inc.,_
_51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA._
- - -

Index
=====

- [`net.newcfg`](#netcfg--netnewcfg-)
- [`netcfg:close`](#succ--netcfgclose-)
- [`netcfg:getarpentry`](#mac--netcfggetarpentry-device--ip)
- [`netcfg:getdevname`](#name--netcfggetdevname-index)
- [`netcfg:getdevindex`](#index--netcfggetdevindex-device)
- [`netcfg:getdevattrib`](#value--netcfggetdevattrib-device--name)
- [`netcfg:setdevattrib`](#success--netcfgsetdevattrib-device--name--value)
- [`netcfg:getdevaddr`](#address--netcfggetdevaddr-device)
- [`netcfg:setdevaddr`](#success--netcfgsetdevaddr-device--address)
- [`netcfg:getdevdstaddr`](#address--netcfggetdevdstaddr-device)
- [`netcfg:setdevdstaddr`](#success--netcfgsetdevdstaddr-device--address)
- [`netcfg:getdevbroadaddr`](#address--netcfggetdevbroadaddr-device)
- [`netcfg:setdevbroadaddr`](#success--netcfgsetdevbroadaddr-device--address)
- [`netcfg:getdevhwaddr`](#address--netcfggetdevhwaddr-device)
- [`netcfg:setdevhwaddr`](#success--netcfgsetdevhwaddr-device--address)
- [`netcfg:getdevnetmask`](#address--netcfggetdevnetmask-device)
- [`netcfg:setdevnetmask`](#success--netcfgsetdevnetmask-device--address)
- [`netcfg:getdevmtu`](#bytes--netcfggetdevmtu-device)
- [`netcfg:setdevmtu`](#success--netcfgsetdevmtu-device--bytes)

Contents
========

CUJO Network Config
-------------------

This library provides generic functions for configuration of network devices.
Unless otherwise noted, in case of errors, all functions described below return `nil`, followed by an error message and a error number.

### `netcfg = net.newcfg ()`

Returns a new network configurator object which can be used to access the configuration parameters of network devices.

### `succ = netcfg:close ()`

Closes the network configurator and frees any resource associated with the configurator.

### `mac = netcfg:getarpentry (device, ip)`

Searches for IP address `ip` in the ARP table of device with name `device`.
It returns the MAC address in the entry, or `nil` if there is no entry in the ARP table for that IP.

### `name = netcfg:getdevname (index)`

Returns a string with the name of the network device with index `index`.

### `index = netcfg:getdevindex (device)`

Returns index corresponding to the network device with name `device`.

### `value... = netcfg:getdevattrib (device [, name...])`

Returns the attributes of device with name `device`.
The arguments `name...` are any of the following strings, which define different network device attributes:

- `"up"`          Interface is running.
- `"broadcast"`   Valid broadcast address set.
- `"debug"`       Internal debugging flag.
- `"loopback"`    Interface is a loopback interface.
- `"pointopoint"` Interface is a point-to-point link.
- `"running"`     Resources allocated.
- `"noarp"`       No ARP protocol, L2 destination address not set.
- `"promisc"`     Interface is in promiscuous mode.
- `"notrailers"`  Avoid use of trailers.
- `"allmulti"`    Receive all multicast packets.
- `"master"`      Master of a load balancing bundle.
- `"slave"`       Slave of a load balancing bundle.
- `"multicast"`   Supports multicast
- `"portsel"`     Is able to select media type via ifmap.
- `"automedia"`   Auto media selection active.
- `"dynamic"`     The addresses are lost when the interface goes down.
- `"lower_up"`    Driver signals L1 up (since Linux 2.6.17)
- `"dormant"`     Driver signals dormant (since Linux 2.6.17)
- `"echo"`        Echo sent packets (since Linux 2.6.25)

For each argument `name...` this function returns a boolean indicating if the value of the attribute for device with name `device`.

### `success = netcfg:setdevattrib (device [, name, value...])`

Sets the attributes of device with name `device`.
For each pair of argument `name, value` provided, `name` must be a string with the name of the attribute to be set and `value` must be a boolean that defines the value of the attribute.
The attributes names are the same listed in operation `getdevattrib`.

This function returns `true` on success.

### `address = netcfg:getdevaddr (device)`

Returns a string with the address of device with name `device`.

### `success = netcfg:setdevaddr (device, address)`

Sets the address in string `address` as the address of device with name `device`.

This is a privileged operation.

### `address = netcfg:getdevdstaddr (device)`

Returns a string with the destination address of a point-to-point device with name `device`.

### `success = netcfg:setdevdstaddr (device, address)`

Sets the address in string `address` as the destination address of a point-to-point device with name `device`.

This is a privileged operation.

### `address = netcfg:getdevbroadaddr (device)`

Returns a string with the broadcast address of device with name `device`.

### `success = netcfg:setdevbroadaddr (device, address)`

Sets the address in string `address` as the broadcast address of device with name `device`.

This is a privileged operation.

### `address = netcfg:getdevhwaddr (device)`

Returns a string with the hardware (MAC) address of device with name `device`.

### `success = netcfg:setdevhwaddr (device, address)`

Sets the address in string `address` as the hardware (MAC) address of device with name `device`.

This is a privileged operation.

### `address = netcfg:getdevnetmask (device)`

Returns a string with the network mask address of device with name `device`.

### `success = netcfg:setdevnetmask (device, address)`

Sets the address in string `address` as the network mask address of device with name `device`.

This is a privileged operation.

### `bytes = netcfg:getdevmtu (device)`

Returns the size of maximum transmission unit (MTU) of device with name `device`.

### `success = netcfg:setdevmtu (device, bytes)`

Sets the size of maximum transmission unit (MTU) of device with name `device`.

This is a privileged operation.
