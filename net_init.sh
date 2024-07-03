#!/bin/bash

sudo ip link delete br0 type bridge

sudo brctl addbr br0
sudo ip link set dev br0 up
sudo ip addr add 192.168.31.1/24 dev br0
