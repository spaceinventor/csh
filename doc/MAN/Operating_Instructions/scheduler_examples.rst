


This section describes command examples to interact with the scheduler and named commands systems of libparam.

**cmd new**

Create a new get or set command queue. 


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> cmd new get example
         | Initialized new command: example
      

**cmd add**

Add a get or set command to the current queue.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>2>#down:example> cmd add param1
         | cmd new get example
         | cmd add -n 2 param1
      .. csh-prompt:: host>2>#down:example> cmd add param2
         | cmd new get example
         | cmd add -n 2 param1
         | cmd add -n 2 param2
      
**cmd run**

Run the current command queue.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>2>#down:example> cmd run
         | param1           	= 42    
         | param2           	= 0
      
**cmd done**

Exit the current command queue.

**cmd server upload**

Upload the current command queue to the server, with <name>.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> cmd server upload example
         | Command added:
         | cmd new set set1
         | cmd add -n 2 param2 21
      
**cmd server download**

Download a command queue from the server. NOTE: does only work with a set queue.

**cmd server list**

List all named commands stored at CSP node <server>.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> cmd server list
         | Received list of 1 saved command names:
         |  1 - example

**cmd server rm**

Remove the named command <name> stored at CSP node <server>. NOTE: Inputting “RMALLCMDS” executes a “remove all” function, clearing all named commands stored at the server.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> cmd server rm -a RMALLCMDS
         | Deleted n commands


**schedule push**

Push the current set queue to a scheduler service running on the CSP node <server>, to be executed at CSP node <host> in <time> seconds (or a unix timestamp in seconds). The latency buffer field is used to limit how late a queue can be executed if something prevents it from being executed at the specified time. Inputting <latency buffer> = 0 disables this feature for that schedule. The response message includes the unique ID of the schedule entry, between 0 and 65534.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> schedule push 3 10 1673356393 0
         | Queue scheduled with id 5:
         | cmd new set example
         | cmd add -n 10 param1 42

**schedule list**

Request a list of the schedule currently saved in the scheduler service running at CSP node <server>. If the schedule is long, the response will be split into multiple packets. Each schedule entry in the list includes a unique ID and when it is scheduled for.

**schedule show**

Request the details of the schedule entry with ID <id> stored at CSP node <server>.

**schedule rm**

Remove the requested schedule with ID <id> from the CSP node <server>. NOTE: Inputting ID = -1 is a “remove all” command which will clear the entire schedule.


**schedule reset**

Reset the scheduler service meta-data, i.e. reset the last id variable to start counting new IDs from a different number. NOTE: This can result in non-unique IDs if run on a server with active schedule entries.

**schedule cmd**

Schedule a named command <name> stored on node ID <server> to be executed at node ID <host> in <time> seconds (or a unix timestamp in seconds). Latency buffer is described under “schedule push”.

Example of scheduling a command queue named “example” on <server> 1, to be executed on <host> 8 in 100 seconds with <latency buffer>.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> sschedule cmd 1 example 8 100 0


**Complete scheduler example**

This example combines the previous examples such that a new command is created, uploaded to a server, and scheduled for execution.

A CSH session creates a command queue locally, which is then uploaded to an On Board Computer (node 1),  where it is then scheduled for execution on module (node 8).

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> node 8
      .. csh-prompt:: host>8> list download
         | Got param: param1:8[1]
         | Got param: param2:8[1]
      .. csh-prompt:: host>8> cmd new set example
      .. csh-prompt:: host>8>#up:example> cmd add param1 21
      .. csh-prompt:: host>8>#up:example> cmd add param2 42
      .. csh-prompt:: host>8>#up:example> cmd server upload -s 1 example
      .. csh-prompt:: host>8>#up:example> scheduler cmd 1 example 8 1673356393 0
