
In order to launch in shell / interactive mode, start csh without a command. Run csp init. Then add an interface with one of the csp add commands and choose a CSP address that will not conflict with any devices.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0

   * -

      .. csh-prompt:: >>$ csh -h
         | usage: csh -i init.csh
         | In CSH, type 'manual' to access CSH manuals
         | 
         | Copyright (c) 2016-2025 Space Inventor A/S <info@space-inventor.com>
      .. csh-prompt:: >>$ csh 
      .. csh-prompt:: host>>csp add zmq -d 8 localhost
         | ZMQ init ZMQO: addr: 8, pub(tx): [tcp://localhost:6000], sub(rx): [tcp://localhost:7000]


.. class:: centered

*Launch csh*