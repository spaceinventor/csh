
The following commands are part of an apm that needs to be installed and then loaded with apm load. 



**hk retrieve**

Pulls data from the housekeeping server. Giving the argument -o will set the hk timeoffset for the selected node. And print the time which can be used to manually set the hk timeoffset with the hk timeoffset command.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> hk retrieve
         | Timestamp 393189
         | 106:212  ext_temp         	= 26.185358

      .. csh-prompt:: host>6> hk retrieve -t 100 -s -N 3
         | Timestamp 100
         | 106:212  ext_temp         	= 25.204676     
         | Timestamp 90
         | 106:212  ext_temp         	= 25.145563      
         | Timestamp 80
         | 106:212  ext_temp         	= 25.116022         

**hk timeoffset**

The housekeeping data’s timestamp starts from the satellite epoch (when the hk server first begins pulling data). Using the current unix timestamp minus the current hk timestamp we find: 1679563632 − 395553 = 1679168079

You can set hk timeoffset for multiple hk nodes.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>135> hk timeoffset 1679168079
         | Setting new hk node 135 EPOCH to 1679168079
      .. csh-prompt:: host>136> hk timeoffset 1679168079
         | Setting new hk node 136 EPOCH to 1679168079    