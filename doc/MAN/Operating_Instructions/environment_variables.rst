Environment variables in CSH
-----------------------------

CSH provides handling of environment variables, making it easier to perform repetitive tasks where only parameter values differ.
The environment variable related commands all start with the ``var`` prefix and are:

* ``var set``: define or update an environment variable
* ``var unset``: remove a variable from the environment 
* ``var clear``: remove all variables from the environment 
* ``var get``: print the value of a particular variable
* ``var show``: show all the variables currently defined in the environment
* ``var expand``: perform variable expansion in a string

See the `Built-in commands`_ section for a description of these commands.

Environment variables examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Defining a new variable:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> var set MY_VARIABLE 1

|


- Printing the value of a variable:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> var get MY_VARIABLE

|


- Using a variable in a command:

.. class:: table

.. list-table::
   :widths: 100
   :header-rows: 0
   
   * - 
      .. csh-prompt:: host>6> watch -n 5 ping $(MY_VARIABLE)


