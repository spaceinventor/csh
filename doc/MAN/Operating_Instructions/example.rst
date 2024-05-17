
1. Connect your module to a CAN dongle (PCAN IPEH-2022 with recommended galvanic isolation) and to a PC running Linux with the socketcan driver.
2. Install our custom software csh (https://github.com/spaceinventor/csh).
3. Once this is configured, open a terminal on the Linux PC and run csh.
4. The command `ident 16383` (broadcast address) will display all nodes on the local network.
5. Once the node where your module is located is identified, simply navigate to it using the command `node <node num>`.
6. Use `list download` to download a list of parameters of your module. Use `pull` to retrieve the value of all parameters.
7. Use `set` to set a specific parameter to a value. For more examples and information about CSH, refer to the following sections of this manual.


