Requirements
----------------------------------

In order to build you need to install the following requirements:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0

   * -
      .. csh-prompt:: >>$ sudo apt install git build-essential libsocketcan-dev
         |can-utils libzmq3-dev libyaml-dev pkg-config fonts-powerline python3-pip libelf-dev
      .. csh-prompt:: >>$ sudo pip3 install meson ninja


.. class:: centered

*Install Requirements*

Build and install
----------------------------------

The software is available as open source on github.

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0

   * -

      .. csh-prompt:: >>$ git clone https://github.com/spaceinventor/csh.git

      .. csh-prompt:: >>$ cd csh

      .. csh-prompt:: >>$ git submodule update --init --recursive

      .. csh-prompt:: >>$ ./configure

      .. csh-prompt:: >>$ ./build

.. class:: centered

*Clone, configure, build and install software*


Launch software
----------------------------------

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

Init configuration
----------------------------------

In order to avoid configuring csp and interfaces on every launch an init.csh file can be created. CSH will look for one in your home directory or you can select one at launch with the -i argument. In the init folder, examples can be found for different interface configurations.




