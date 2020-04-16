#!/bin/sh
sudo ip link set dev can0 down
sudo ip link set dev can0 up type can bitrate 1000000 restart-ms 100 sample-point 0.750 sjw 2
sudo ip link set dev can0 txqueuelen 1000
sudo watch -n1 "ip --details --statistics link show can0"

