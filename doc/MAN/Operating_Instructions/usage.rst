CSH serves for module testing and operations. This section outlines the procedure for module testing, provides examples of command usage, and presents a list of commands for use in CSH.

Shell interface
----------------------------------

CSH runs a simple command line interface (slash). After starting CSH you will see a prompt. For example to ping a node:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: >>$ csh
      .. csh-prompt:: host>>node 212
      .. csh-prompt:: host>212>ping
         |Ping node 212 size 0 timeout 1000: Reply in 14...
      .. csh-prompt:: host>obc@212> ident
         |IDENT 212
         | obc-hk
         | FLASH-1
         | v1.2-1-gd958d8e+
         | Mar 17 2023 12:18:08

.. class:: centered

*Example of ping and ident commands*


In-application help texts
----------------------------------

Most commands include a help text that is accessible by executing the command followed by -h.

Example module testing procedure using CSH
--------------------------------------------

1. Connect your module to a CAN dongle (PCAN IPEH-2022 with recommended galvanic isolation) and to a PC running Linux with the socketcan driver.
2. Install our custom software csh (https://github.com/spaceinventor/csh).
3. Once this is configured, open a terminal on the Linux PC and run csh.
4. The command `ident 16383` (broadcast address) will display all nodes on the local network.
5. Once the node where your module is located is identified, simply navigate to it using the command `node <node num>`.
6. Use `list download` to download a list of parameters of your module. Use `pull` to retrieve the value of all parameters.
7. Use `set` to set a specific parameter to a value. For more examples and information about CSH, refer to the following sections of this manual.


