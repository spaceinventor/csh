Space Inventor is relasing CSH together with an APM called CSH SI that provides commands to interface with Space Inventor hardware. The following documentation describes the commands provided by the CSH SI APM.

**node add**

When loading the APM, this command is extended to include a Space Inventor PDU-P4 node and channel number as arguments. When registering this additional information, CSH is able to power on and off the module by remote controlling the PDU.

The following example shows how to add a Space Inventor OBC module having node 6, connected to channel 2 of a PDU-P4 with the hostname ``pdu1-a``.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> node add -p pdu1-a -c 2 obc
      .. csh-prompt:: host>obc@6> 

**power on/off**

Having a node registered with PDU information allows the user to power cycle the module remotely using the ``power on`` and ``power off`` commands as shown in the following example. The explicit obc reference can be excluded, but preselcting the node by using ``node obc`` before starting the operations.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> power on obc
      .. csh-prompt:: host>> sleep 1000
      .. csh-prompt:: host>> ping obc
         | Ping node 6 size 1 timeout 1000: Reply in 1 [ms]
      .. csh-prompt:: host>> power off obc

**conf get**

This command receives and stores all persistent configuration parameters from module compliant with Space Inventor Parameter System. Optionally, the command can reboot the module into each available firmware slot to record the firmware version, and perform a list download to create a complete representation of the module configuration.

The command can retrieve the configuration from multiple modules by a single command, by providng a list of nodes separated by commas.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>bat@8> conf get -r -l
         |
         | IDENT 8
         | bat
         | samc21 01]
         | v1.0-32-g61cfbc9
         | Apr 12 2024 16:04:12
         |
         | IDENT 8
         | bat
         | samc21 [1]
         | v1.0-32-g61cfbc9
         | Apr 12 2024 16:04:46
         |
         | Got param: temp:8[1]
         | Got param: boot_cnt:8[1]
         | Got param: boot_cur:8[1]
         | ...
         | Got param: heater_auto:8[1]
         | Got param: cell_voltage:8[8]
         | Got param: bus_current:8[1]
         |
         | Configuration is stored into ./configuration and ./paramdefs
      .. csh-prompt:: host>bat@8>
