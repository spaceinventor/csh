#!/bin/sh
sudo ip link set dev can0 down
sudo ip link set dev can0 up type can bitrate 1000000 restart-ms 100
sudo ip link set dev can0 txqueuelen 100
sudo watch -n1 "ip --details --statistics link show can0"

