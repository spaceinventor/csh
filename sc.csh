csp init

param_server start

cmd new set test
cmd add csp_print_packet 1
cmd server upload -v 2

cmd new set test2
cmd add csp_print_packet 0
cmd server upload -v 2

cmd server list -v 2

# Download command 'test'
cmd server download -v 2 0x75E7696D

# Remove command 'test'
cmd server rm -v 2 0x75E7696D

cmd server list -v 2

# Enable print in 5 sec
cmd new set testsch
cmd add csp_print_packet 1
schedule push -v 2 5

# Disable CSP print (command 'test2') in 10 sec
schedule cmd -v 2 0xC86FB189 10

# Enable ping to check CSP print
watch ping
