It's a simple application to forward IEEE 1588v2 packets between two network
interfaces.

usage:
1. gcc -o ptpproxy ptpproxy.c  -pthread
2. ./ptpproxy br0 eth0