
Here is a list of the most used commands together with small examples. The list is subject to change.

**exit**

Exit csh (ctrl+d may also be used)

**history**

Show command history

**help**

Print out list of commands

**csp init**

Initialize CSP in CSH. Optionally select CSP version.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> csp init
         | Version 2
         | Hostname: host
    

**csp add can**

Register new CAN interface in CSH.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> csp add can1
         | INIT CAN0: device:[can0],bitrate: 1000000, 
         | ...promisc:1
    
**csp add zmq**

Initialise a new ZMQ interface.
.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> csp add zmq1 localhost
         | ZMQ init ZMQ0: addr: 1, pub(tx): 
         | ...[tcp://localhost:6000],sub(rx):
         | ...[tcp://localhost:7000]

    
**csp add route**

Add new routes to the CSP routing table in CSH.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> csp route add 64/8 CAN0
         | Added route 64/8 ZMQ0
         | ...[tcp://localhost:6000],sub(rx):
         | ...[tcp://localhost:7000]

**csp scan**

Scan all nodes for devices.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> csp scan
         | CSP SCAN [0:16382]
         | Found something on addr 0...
         | lenovo
         | #36~22.04.1-Ubuntu SMP PREEMP
         | 5.19.0-35-generic
         | Mar 22 2023 14:17:59
         |
         | Found something on addr 212...
         | obc-hk
         | FLASH-1
         | v1.2-1-gd958d8e+
         | Mar 17 2023 12:18:08

**prometheus start**

Start the prometheus node exporter to forward all parameter request to Promotheus.

Use the hk timeoffset command to set the node of hk server for the housekeeping sniffer, to forward historical data requested from house keeping service on a satellite.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> prometheus start
         | Prometheus exporter listening on port 9101
      .. csh-prompt:: host>> prometheus start -n 6
         | Initialising house keeping sniffer for HK node 6

**vm start**

Starts pushing parameters to a victoria metrics database.

Use the hk timeoffset command to set the node of hk server for the housekeeping sniffer, to forward historical data requested from house keeping service on a satellite.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> vm start localhost
         | Connection established to http://localhost:8428 
      .. csh-prompt:: host>> vm start -u username -p password -s -P 8427 
         | hostname.com


**info**

Provides CSP info for the local node. First the routing table, then the connection table and finally interface statistics.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> info
         | [00 0x556b4da62120] S:0, 0 -> 0, 17 -> 1 (17) fl 1
         | [01 0x556b4da640f8] S:0, 0 -> 0, 18 -> 1 (18) fl 1
         | [02 0x556b4da660d0] S:0, 0 -> 0, 19 -> 1 (19) fl 1
         | ...
         | [16 0x556b4da81ea0] S:0, 0 -> 0, 33 -> 1 (33) fl 1
         | [17 0x556b4da83e78] S:0, 0 -> 0, 34 -> 1 (34) fl 1
         | [18 0x556b4da85e50] S:0, 0 -> 0, 35 -> 1 (35) fl 1
         | [19 0x556b4da87e28] S:0, 0 -> 0, 36 -> 1 (36) fl 1
         | LOOP       addr: 0 netmask: 14
       	 |            tx: 00026 rx: 00026 txe: 00000 rxe: 00000
       	 |            drop: 00000 autherr: 00000 frame: 00000
       	 |            txb: 104 (104B) rxb: 104 (104B)
         | 
         | ZMQ0       addr: 107 netmask: 8
       	 |            tx: 00070 rx: 00086 txe: 00000 rxe: 00000
       	 |            drop: 00000 autherr: 00000 frame: 00000
       	 |            txb: 2117 (2K) rxb: 5033 (4K)


**node**

Sets default/environment node for most commands. Giving the node as a positional argument when running a command will take precedence over the default environment node set.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> node 6
      .. csh-prompt:: host>6> 

**upload**

Upload a file to a memory.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: >> $ echo "HELLO WORLD" >> hello.txt
      .. csh-prompt:: host>6> upload hello.txt 0x30001000
         | Upload from hello.txt to node 6 addr 0x30001000 with timeout 2000
         | Size 12
         |  . - 0 K  
         | Uploaded 12 bytes in 0.003 s at 4000 Bps

**download**

Download memory to a file.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> download 0x30001000 12 hello2.txt
      .. csh-prompt:: host>6> upload hello.txt 0x30001000
         | Download from 6 addr 0x30001000 to hello2.txt with timeout 10000
         |  . - 0 K
         | Downloaded 12 bytes in 0.007 s at 1714 Bps
      .. csh-prompt:: host>6> exit
      .. csh-prompt:: >> $ cat hello2.txt
         | HELLO WORLD     


**pull**

Get all parameters from a remote node.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> pull
         | 130:6  adc_temp             = 21769
         | 303:6  alarm_dbg            = 1
         | 25:6  boot_cnt             = 451
         | 24:6  boot_cur             = 0
         | 26:6  boot_err             = 32
         | 21:6  boot_img0            = 0
         | 20:6  boot_img1            = 0
         | 384:6  ch_protect           = 0
         | 13:6  csp_can_pwrsave      = 1
         | 11:6  csp_can_speed        = 1000000
         | 10:6  csp_node             = 6
         | 12:6  csp_rtable           = ""
         | 140:6  dac_enabled          = [0 0 0 0 0 0]
         | 164:6  efficiency           = 0.0000

**set**

Set a single parameter.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> set gndwdt 10000
         | 1:6  gndwdt               = 10000 uint32[1]

**get**

Get a single parameter.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> get gndwdt 10000
         |    1:6  gndwdt               = 9997

**list download**

Download a list of remote parameters.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> list download
         | Got param: adc_temp[1]
         | Got param: alarm_dbg[1]
         | Got param: boot_cnt[1]
         | Got param: boot_cur[1]
         | Got param: boot_err[1]
         | ...
         | Got param: tlm_vmax[1]
         | Got param: tlm_vmin[1]
         | Got param: v_in[6]
         | Got param: v_out[1]
         | Received 81 parameters


**list**

Print current parameter list of selected node. Use -n -1 to list all remote parameters from all nodes. 


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> list 
         | 20:6  boot_img1        	= 0             	 
         | 21:6  boot_img0        	= 0            
         | 22:6  boot_img2        	= 0   
         | 23:6  boot_img3        	= 0   
         | 24:6  boot_cur        	= 0   
         | 25:6  boot_cnt        	= 0   
         | 26:6  boot_err        	= 0   
         | 1:6  gndwdt          	= 0   
         | 51:6  csp_buf_out       	= 0   
         | ...


**watch**

Repeat a command periodically.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> watch -n 1000 "ping"
         | Executing "ping" each 1000 ms - press <enter> to stop             	 
         | Ping node 6 size 1 timeout 1000: Reply in 2 [ms] 
         | Ping node 6 size 1 timeout 1000: Reply in 8 [ms]
         | Ping node 6 size 1 timeout 1000: Reply in 2 [ms]

**time**

Remote timesync.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> time
         | Remote time is 1516625445.622655490 (diff 107 us)          	 


**poke**

Manipulate remote memory (<200 bytes)


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> poke 0x30001000 DEADBEEF
         | Base16-decoded "DEADBEEF" to:
         | Poke at address 0x30001000
         | 0x7ffc60726e67  de ad be ef      
         | ...
       	 


**peek**

Request a small (<200 bytes) piece of memory.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> peek 0x30001000 16
         | Peek at address 0x30001000 len 16
         |  0x7ffc60726e67  48 45 4c 4c 4f 20 57 4f 52 4c 44 0a 00 00 00 00 
         | HELLO WORLD.....   
    
       	 

**ifstat**

Remotely request interface statistics. For a combined overview of all interfaces, use the parameter csp_print_cnf that is available on most modules.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> ifstat CAN
         | CAN 	  tx: 75840 rx: 81818 txe: 00000 rxe: 00000
         |        drop: 00000 autherr: 00000 frame: 06176
         |        txb: 3265270 rxb: 3321911  
    
       	 

**ident**

Responds with some system info. Hostname, Vendor, Revisions and Timestamp of build. Using ident on a broadcast node or global broadcast (16383) can be used as a csp scan to find all devices within the local network.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> ident
         | IDENT 6
         |    obc-hk
         |    FLASH-1
         |    v1.2-1-gd958d8e+
         |    Mar 17 2023 12:18:08    
       	 
      .. csh-prompt:: host>obc-hk@6> ident 127
         | IDENT 107
         |    lenovo
         |    #36-22.04.1-Ubuntu SMP PREEMP
         |    5.19.0-35-generic
         |    Mar 22 2023 14:17:59   

         | IDENT 89
         |    lab
         |    #66-Ubuntu SMP Fri Jan 20 14:
         |    5.15.0-60-generic
         |    Oct 26 2022 16:23:29 

**uptime**

Responds with the system uptime.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> uptime
         | Uptime of node 6 is 10 s
        

**buffree**

Request the number of remaining CSP buffers on a node.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> buffree
         | Free buffers at node 6 is 9
        
**reboot**

Reboot a remote node.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> uptime
         | Uptime of node 6 is 10 s
      .. csh-prompt:: host>6> reboot
      .. csh-prompt:: host>6> uptime       
         | Uptime of node 6 is 0 s

**ping**

Send a ping and wait for a response from the target.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> ping
         | Ping node 6 size 1 timeout 1000: Reply in 1 [ms]



**vmem**

List vmem areas on remote node:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> vmem
         | Requesting vmem list from node 6 timeout 1000 version 2
         |  0: sched 0x31001000 - 4096 typ 8
         |  1: comma 0x31002000 - 4096 typ 8
         | 2: hk_li 0x31003000 - 20480 typ 8
         | 3: hk_co 0x31000500 - 1280 typ 8
         | 4: stdbu 0x2045f100 - 3584 typ 1
         | 5: fram  0x30000000 - 32768 typ 2
         | 6: fl3   0x580000 - 524288 typ 4
         | 7: fl2   0x500000 - 524288 typ 4
         | 8: fl0   0x404000 - 507904 typ 4
         | 9: csp   0x31000000 - 84 typ 2
         | 10: btldr 0x31000400 - 16 typ 2



**list add**

Add a remote parameter without downloading from the device.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> list add -c "FRAM+C" -m "Rt" hk_next_timestamp 154 uint32

**switch**

Reboot into another boot image.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> switch 1
         | Switching to flash 1
         | Will run this image 1 time
         |cmd new set
         | Rebooting..........................
         | |obc-hk
         | |FLASH-1
         | |v1.2-1-gd958d8e+
         | |Mar 17 2023 12:18:08


**program**

Program a slot, with automatic search for valid binaries in the current working directory. Optionally specify a file with the -f option.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> program 0
         | Setting rdp options: 3 10000 5000 2000 2
         | node 16
         |      Requesting VMEM name: fl0...
         |      Found vmem
         |               Base address: 0x404000
	     |               Size: 507904
         | Searching for valid binaries
         | 0: ./obc-0.bin
         |        
         | ABOUT TO PROGRAM: ./obc-0.bin
         | 
         | |obc-hk
         | |FLASH-1
         | |v1.2-1-gd958d8e+
         | |Mar 17 2023 12:18:08
      .. csh-prompt:: host>6> yes
         | Upload 82664 bytes to node 6 addr 0x404000
         | ................................ - 6 K
         | ................................ - 78 K
         | ................................ - 81 K
         | Uploaded 82664 bytes in 5.950 s at 13893 Bps
         | ................................ - 6 K
         | ................................ - 78 K
         | ................................ - 81 K
         | Downloaded 82664 bytes in 4.551 s at 18163 Bps

The normal operation of the program command is to upload the entire firmware image to the module and then download it back to the CSH terminal, for bitwise comparison. This can in some circumstances prove to be very time consuming. For this reason, the system can be instructed to use a different approach using a simple CRC-32 checksum calculation on “both sides” of the communication channel. Specifying the -c option on the command line will instruct the CSH client to do a CRC-32 calculation on the firmware file prior to uploading it to the module. When the upload process has completed, the module is instructed to do the same CRC-32 calculation on all the data received and send back the result (only 32-bits) to the CSH client for verification. For this option to succeed, the module has to support the CRC-32 calculation feature, otherwise the program operation will end with a communication error.


**sps**

Temporarily switch into a specific slot, program another slot and switch into the newly programmed slot.
Here we are running sps while in slot 1, then rebooting into slot 0, programming slot 1 finally rebooting into slot 1.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> ident
         | IDENT 6
         | obc-hk
         | FLASH-1
         | v1.2-1-gd958d8e+
         | Mar 17 2023 12:18:08
      .. csh-prompt:: host>6> sps 0 1
         | Setting rdp options: 3 10000 5000 2000 2
         |   Switching to flash 0
         |   Will run this image 1 times
         | cmd new set
         |   Rebooting........................................
         | |obc-hk
         | |FLASH-1
         | |v1.2-1-gd958d8e
         | |Feb 22 2023 13:55:31
         | Requesting VMEM name: fl1...
         | Found vmem
         |               Base address: 0x480000
	     |               Size: 524288
         | Searching for valid binaries
         | 0: ./obc-1.bin
         | ABOUT TO PROGRAM: ./obc-1.bin
         | |obc-hk
         | |FLASH-0
         | |v1.2-1-gd958d8e
         | |Feb 22 2023 13:55:31
         | Upload 82664 bytes to node 6 addr 0x404000
         | ................................ - 6 K
         | ................................ - 78 K
         | ................................ - 81 K
         | Uploaded 82664 bytes in 5.950 s at 13893 Bps
         | ................................ - 6 K
         | ................................ - 78 K
         | ................................ - 81 K
         | Downloaded 82664 bytes in 4.551 s at 18163 Bps
         | Switching to flash 1
         | Will run this image 1 times
         | cmd new set
         |   Rebooting........................................
         | |obc-hk
         | |FLASH-1
         | |v1.2-1-gd958d8e
         | |Feb 22 2023 13:55:41




**stdbuf2**

Retrieve the stdout buffer of node and clear it.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> stdbuf2
         | bootmsg: obc-hk Feb 15 2023 08:29:19 slot: 0, cause: SOFT
         | |Feb 15 2023 08:29:18   

**vts init**

Send ADCS q_hat and position parameters to vts timeloop software. Specify the adcs node with -n.
Server ip and port can be changed from defaults with -s and -p.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> vts init -n 300
         | Streaming data to VTS at 127.0.0.1:8888

**apm load**

Load a csh apm (addin, plugin, module). Will automatically search in $HOME/.local/lib/csh folder for installed APMs.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> apm load
         | Loaded: /home/user/.local/lib/csh/libcsh_hk.so




