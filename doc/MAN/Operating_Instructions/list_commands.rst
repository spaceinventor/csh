List of commands
-----------------------------

Build-in commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. class:: table

.. list-table::
   :widths: 20 80
   :header-rows: 1
   
   * - Name
     - Usage
   * - apm load
     - [OPTIONS...] -f <filename> -p <pathname>  
   * - apm info
     - [OPTIONS...] <search>
   * - buffree
     - [OPTIONS…] [node]

   * - cd
     - 

   * - cmd
     - 

   * - cmd add
     - [OPTIONS...] <name>[offset] [value]

   * - cmd done
     - [OPTIONS...] -f <filename> -p <pathname>  

   * - cmd new
     - [OPTIONS...] <get/set> <cmd name>

   * - cmd run
     - [OPTIONS...] 

   * - cmd server download
     - [OPTIONS...] <name>

   * - cmd server list
     - [OPTIONS...] 


   * - cmd server rm
     - [OPTIONS...] <name>

   * - cmd server upload
     - [OPTIONS...] <name>

   * - csp add can
     - [OPTIONS...] <addr>

   * - csp add kiss
     - [OPTIONS...] <addr>

   * - csp add route
     - [OPTIONS...] <addr>/<mask> <ifname>

   * - csp add tun
     - [OPTIONS...] <ifaddr> <tun src> <tun dst>

   * - csp add udp
     - [OPTIONS...] <addr> <server>

   * - csp add zmq
     - [OPTIONS...] <addr> <server>

   * - csp init
     - [OPTIONS...] 

   * - csp scan
     - [OPTIONS...] 

   * - download
     - [OPTIONS...] <address> <length> <file>

   * - exit
     -

   * - get
     - [OPTIONS...] <name>[offset]

   * - help
     - help [command]

   * - history
     -

   * - ident
     - [OPTIONS...] [node]

   * - ifstat
     - [OPTIONS...] <ifname>

   * - info
     - 

   * - list
     - [OPTIONS...] [name wildcard=*]

   * - list add 
     - [OPTIONS...] <name> <id> <type>

   * - list download
     - [OPTIONS...] 

   * - list forget
     - [OPTIONS...] 

   * - list save
     - [OPTIONS...] [name wildcard=*]

   * - list upload
     - <address> [OPTIONS...]

   * - ls
     - 

   * - node
     - 

   * - node add
     - [OPTIONS...] <name>

   * - node list
     -

   * - node save
     -

   * - param_server start
     -

   * - peek
     - [OPTIONS...] <addr> <len>

   * - ping
     - [OPTIONS...] [node]

   * - poke
     - [OPTIONS...] <addr> <data base16>

   * - program
     - [OPTIONS...] <slot>

   * - prometheus start
     - [OPTIONS...]

   * - pull
     - 

   * - rdp opt
     - 

   * - reboot
     - [OPTIONS...][node]

   * - resbuf
     - [OPTIONS...] 

   * - run
     - [OPTIONS...] <filename>

   * - sleep
     - 

   * - schedule cmd
     - <server> <name> <host> <time> <latency buffer> [timeout]

   * - schedule list
     - <server> [timeout]

   * - schedule push
     - <server> <host> <time> <latency buffer> [timeout]

   * - schedule reset
     - <server> <last id> [timeout]

   * - schedule rm
     - <server> <id> [timeout]

   * - schedule show
     - <server> <id> [timeout]

   * - set
     - [OPTIONS...] <name>[offset] <value> 

   * - shutdown
     - [OPTIONS...] [node]

   * - sps
     - [OPTIONS...]<slot>

   * - set
     - [OPTIONS...] <name>[offset] <value> 


   * - shutdown
     - [OPTIONS...] [node]


   * - sps
     - [OPTIONS...]<slot>

   * - set
     - [OPTIONS...] <name>[offset] <value> 


   * - shutdown
     - [OPTIONS...] [node]

   * - sps
     - [OPTIONS...]<slot>

   * - stdbuf
     - [OPTIONS...]

   * - stdbuf2
     - [OPTIONS...] 

   * - switch
     - [OPTIONS...]<slot>

   * - time
     - [OPTIONS...] [timestamp]

   * - timeout
     - <timeout ms>

   * - upload
     - [OPTIONS...] <file> <address>

   * - uptime
     - [OPTIONS...] [node]

   * - vts init
     - 

   * - vm start
     - [OPTIONS...] [node]

   * - vm stop
     -

   * - vmem
     -  [OPTIONS...] 

   * - watch
     -  [OPTIONS...] <command...>


Housekeeping APM (libcsh_hk.so) commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. class:: table

.. list-table::
   :widths: 20 80
   :header-rows: 1
   
   * - Name
     - Usage
   * - hk retrieve
     - [OPTIONS...] 
   * - hk timeoffset
     - [OPTIONS...] 

Cortex APM (libcsh_cortex.so) commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. class:: table

.. list-table::
   :widths: 20 80
   :header-rows: 1
   
   * - Name
     - Usage
   * - csp add leafspace
     - [OPTIONS...] <addr> <NoradID> <mqtt-broker>
   * - csp add cortex_crt
     - [OPTIONS...] <addr> <server>

   * - csp add cortex_hdr
     - [OPTIONS...] <addr> <server>