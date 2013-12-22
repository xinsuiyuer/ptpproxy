It's a simple application to forward IEEE 1588v2 Ethernet multicast packets between two network
interfaces.

![demo](screenshot1.jpg)

usage:

1. gcc -o ptpproxy ptpproxy.c  -pthread
2. ./ptpproxy br0 eth0
