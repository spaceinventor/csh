csp init -v 1
csp add can -c can0 -d 1
csp add can -c can1 -d 1
sleep 1000

node 10
list add -c "1=legacy use, 2=csp version 2" -m "C" -v 2 csp_version 16 u08
set csp_version 2
reboot

node 11
list add -c "1=legacy use, 2=csp version 2" -m "C" -v 2 csp_version 16 u08
set csp_version 2
reboot
