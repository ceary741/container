#!/bin/bash

# arguments
# 1 PID
# 2 ns name
# 3 IP
# 4 bridge nic
# 5 container nic

sudo ip link delete $4
sudo ip link add $4 type veth peer name $5
sudo brctl addif br0 $4

sudo touch /var/run/netns/$2
sudo mount --bind  /proc/$1/ns/net /var/run/netns/$2

sudo ip link set $5 netns $2 
sudo ip netns exec $2 ip link set dev $5 name eth0
sudo ip netns exec $2 ip link set eth0 up
sudo ip netns exec $2 ip addr add $3/24 dev eth0
sudo ip netns exec $2 ip route add default via 192.168.31.1

