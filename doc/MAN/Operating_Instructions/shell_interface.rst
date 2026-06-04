
CSH runs a simple command line interface. After starting CSH you will see a prompt. Assuming an init file is already created, the following is required to ping a node:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: >>$ csh
      .. csh-prompt:: host>>node 212
      .. csh-prompt:: host>212>ping
         |Ping node 212 size 0 timeout 1000: Reply in 14 [ms]
      .. csh-prompt:: host>212> ident
         |IDENT 212
         | obc-hk
         | FLASH-1
         | v1.2-1-gd958d8e+
         | Mar 17 2023 12:18:08
      .. csh-prompt:: host>obc@212>

.. class:: centered

*Example of ping and ident commands*


