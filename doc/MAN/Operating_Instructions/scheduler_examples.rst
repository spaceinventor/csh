


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
