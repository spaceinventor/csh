Environment variables in CSH
-----------------------------

From version XXXX, CSH provides handling of environment variables, making it easier to perform repeatitive tasks where only parameter values differ.
The environment variable related commands all start with the ``var_`` prefix and are:

* ``var_set``: define or update an environment variable
* ``var_unset``: remove a variable from the environment 
* ``var_clear``: remove all variables from the environment 
* ``var_get``: print the value of a particular variable
* ``var_show``: show all the variables currently defined in the environment
* ``var_expand``: perform variable expansion in a string

See the `Built-in commands`_ section for a description of these commands.
