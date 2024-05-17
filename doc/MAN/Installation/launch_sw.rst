
In order to launch in shell / interactive mode, start csh without a command. Run csp init. Then add an interface with one of the csp add commands and choose a CSP address that will not conflict with any devices.


.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0

   * -

      .. csh-prompt:: >>$ csh -h
         | usage: csh -i init.csh[command]
      .. csh-prompt:: >>$ csh 
      .. csh-prompt:: host>> csp init
         | Version 2
         | Hostname: lenovo
         | Model: #36~22.04.1-Ubuntu SMP PREEMPT_DYNAMIC Fri   Feb 17 15:17:25 UTC 2
         | Revision: 5.19.0-35-generic
         | Deduplication: 3
      .. csh-prompt:: host>>csp add zmq -d 8 localhost
         | ZMQ init ZMQO: addr: 8, pub(tx): [tcp://localhost:6000], sub(rx): [tcp://localhost:7000]


.. class:: centered

*Launch csh*