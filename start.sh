#!/bin/bash
mkdir $1
docker pull centos:8
docker export $(docker create centos:8) | tar -C $1 -xvf -
sudo rmdir  /sys/fs/cgroup/$1
sudo mkdir /sys/fs/cgroup/$1
echo "$3 $4" | sudo tee /sys/fs/cgroup/$1/cpu.max
echo "$5M" | sudo tee /sys/fs/cgroup/$1/memory.max
systemd-run --unit=$1 --scope env -i env HOME=/root USER=root TERM=xterm  SHELL=/bin/bash ./main $1 $2

