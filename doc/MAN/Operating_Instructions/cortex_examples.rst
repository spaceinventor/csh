

The following commands are part of an apm that needs to be installed and then loaded with apm load.


**csh add leafspace**

Adds an MQTT interface to the Leafspace ground segment operators MQTT broker. Prerequisites are to have acquired a set of credentials from Leafspace to gain access to their MQTT broker, and to know the NORADID of the satellite to establish communication with.
Here is an example of how to add a MQTT interface thru Leafspace MQTT broker (host: mqtt.leaf.space) to a satellite with NORADID 25667 using <user> and <pass> as credentials. The interface is given a node address of 589.
The info command can then be run to inspect the resulting interface: MQTT0-25667.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> csp add leafspace -u <user> -w <pass> 589 25667 mqtt.leaf.space
      .. csh-prompt:: host>> info
         | MQTT0-25667 addr: 589 netmask: 8 dfl: 0
         |        tx: 00000 rx: 00000 txe: 00000 rxe: 00000
         |        drop: 00000 autherr: 00000 frame: 00000
         |        txb: 0 (0B) rxb: 0 (0B)

This command offers a set of options for controlling how to set up the communication with the MQTT broker. Use the -h option for more information.


**csh add cortex_crt**

Adds an interface with a direct TCP/IP socket connection to a Cortex CRT modem at a specific IP address.


**csh add cortex_hdr**

Adds an interface with a direct TCP/IP socket connection to a Cortex HDR modem at a specific IP address.
