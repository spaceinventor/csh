CSH
===========================================================================

Command Shell for Linux PC's



Description
----------------------------------
CSH is a Linux program designed to interface to Space Inventor modules and satellites using a CSP interface. With a CAN dongle the PC will perform as a first-class citizen on the satellite bus and have full access to all systems.

After launch, CSH is also used to operate the satellite through the radio interface, utilising CSPs routing functionalities.

CSH is written in C, and uses the same software libraries (libcsp and libparam) as Space Inventors subsystems. This ensures full compatibility with the protocol stack as well as the higher layer parameter system.

   

Features
----------------------------------



.. class:: transparenttable

  .. list-table::
    :widths: 50 50 
    :header-rows: 0

    * - • CSP version 1 and version 2 support
      - • Parameter system client
    
    * - • CAN bus, UART, ZMQ and other interfaces
      - • File transfer client

    * - • Space Inventor shell interface
      - • CSP node and shell commands
  
    * - • Remote system operations
      - 




 

Physical Setup
----------------------------------

.. image:: images/pysical_setup.png
   :width: 8 cm

The system consists of a Linux PC with a CAN dongle, and a device under test (the subsystem). There can be several CSP nodes on the bus as well.

