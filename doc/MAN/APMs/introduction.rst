CSH can be extended via Extension Modules (APMs) using C code or python. These modules provide additional commands and features that are not part of the core CSH shell but can be loaded as needed. 

Loading an APM is done by using the ``apm load`` slash command. The command enables the user to load all, or specific APMs available in the system.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>> apm load
         | Loaded: /home/tjessen/.local/lib/csh/libcsh_obc.so
         | Slash command 'node save' is overriding an existing command
         | Slash command 'node list' is overriding an existing command
         | Slash command 'node add' is overriding an existing command
         | Loaded: /home/tjessen/.local/lib/csh/libcsh_si.so
      .. csh-prompt:: host>> 
