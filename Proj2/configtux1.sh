#!/bin/sh

/etc/init.d/networking restart

ifconfig eth0 down
ifconfig eth1 down
ifconfig eth0 up
ifconfig eth0 172.16.50.1/24

route add -net 172.16.51.0/24 gw 172.16.50.254
route add default gw 172.16.50.254

echo "search netlab.fe.up.pt\nnameserver 172.16.2.1" > /etc/resolv.conf
