.. .....................................................................

.. MODULE SPECIFIC STATICS..............................................

.. |description| replace:: C-Shell

.. |module_image| image:: ./images/6U-Tactical-Earth-Observation.png
    :width: 15cm

.. .....................................................................

.. DOCUMENT CONTENT ..............................................

CSH
===========================================================================

Command Shell for Linux PC's

Description
--------------------
.. include:: MAN/Introduction/description.rst

Features
--------------------
.. include:: MAN/Introduction/features.rst

Physical Setup
--------------------
.. include:: MAN/Introduction/physical_setup.rst

.. raw:: pdf

    PageBreak 

Installation
===========================================================================

Requirements
-------------------
.. include:: MAN/Installation/requirements.rst

Build and install
------------------------
.. include:: MAN/Installation/install.rst

Launch software
--------------------
.. include:: MAN/Installation/launch_sw.rst

Initial configuration
------------------------
.. include:: MAN/Installation/init_config.rst

.. raw:: pdf

    PageBreak 

Operating instructions
===========================================================================
.. include:: MAN/Operating_Instructions/usage.rst

Shell interface
-------------------
.. include:: MAN/Operating_Instructions/shell_interface.rst

Example module testing procedure using CSH
--------------------------------------------
.. include:: MAN/Operating_Instructions/example.rst

.. raw:: pdf

    PageBreak oneColumn

List of commands
-----------------------------

Built-in commands
~~~~~~~~~~~~~~~~~

.. partool -q --csv -s builddir/csh | (read -r; printf "%s\n" "$REPLY"; sort) > doc/MAN/Operating_Instructions/builtin_commands.csv

.. csv-table:: Built-in commands
    :file: MAN/Operating_Instructions/builtin_commands.csv
    :widths: 20 20 70
    :header-rows: 1

Housekeeping APM (libcsh_hk.so) commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. include:: MAN/Operating_Instructions/hk_apm.rst

Cortex APM (libcsh_cortex.so) commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. include:: MAN/Operating_Instructions/cortex_apm.rst

.. raw:: pdf

    PageBreak 

Command examples
-----------------------------
.. include:: MAN/Operating_Instructions/command_examples.rst

Housekeeping commands (APM)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. include:: MAN/Operating_Instructions/hk_examples.rst

Cortex CSH (APM)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. include:: MAN/Operating_Instructions/cortex_examples.rst

Scheduler & Named Commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. include:: MAN/Operating_Instructions/scheduler_examples.rst

.. include:: MAN/Operating_Instructions/environment_variables.rst

.. Appendices
.. ===========================================================================

.. .. include:: MAN/Appendices/abbreviation_list.rst





.. .....................................................................
    

