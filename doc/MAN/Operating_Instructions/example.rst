
1. Connect your module to a CAN dongle and to a PC running Linux with the socketcan driver.
2. Install csh from https://github.com/spaceinventor/csh.
3. Run the script `caninit` to initialize the CAN interface
4. Start csh and register the CAN interface
5. The command `ident 16383` (broadcast address) will display all nodes on the local network.
6. Once the node where your module is located is identified, simply navigate to it using the command `node <node num>`.
7. Use `list download` to download a list of parameters of your module. Use `pull` to retrieve the value of all parameters.
8. Use `set` to set a specific parameter to a value. For more examples and information about CSH, refer to the following sections of this manual.
