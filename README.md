### CSP Shell

CSP Shell or `csh` pronounced *seashell* is a command line interpreter for running CSP and `libparam` based applications.

### spaceinventor/satctl changes name to spaceinventor/csh

`satctl` was originally a piece of software under license from 'Satlab A/S'. Since the current
version of 'spaceinventor/satctl' has nothing to do with satlab or the original `satctl`
we are changing the name of both the software and the repository to avoid any misunderstanding.

The process will be gradual to avoid too many broken links so the old repo will remain in place for a while still.
But the software will be called `csh` (pronounced 'seashell') from now on.


### Build

Requirements: libcurl4-openssl-dev build-essential, libsocketcan-dev, can-utils, libzmq3-dev, libyaml-dev, meson, ninja, pkg-config, fonts-powerline, python3-pip, libelf-dev

```
sudo apt install libcurl4-openssl-dev git build-essential libsocketcan-dev can-utils libzmq3-dev libyaml-dev pkg-config fonts-powerline python3-pip libelf-dev
sudo pip3 install meson ninja
```

Sometimes needed:
```
link /usr/sbin/ninja /usr/local/lib/python3.5/dist-packages/ninja
export PATH=~/.local/bin:$PATH
```

Build:
```
git clone --recurse-submodules https://github.com/spaceinventor/csh.git
cd csh
./configure
./install
```

### Run

To setup CAN sudo is required:

```
caninit
```

Then launch two instances
`csh -i conf1.csh`
and
`csh -i conf2.csh`

Then on instance 1 type `ping 2`

Note: you need to setup the two different configuration files, so they can speak to each other, e.g.
```
csp init
csp add can -d 1
```

Special CAN configurations in CSH
=================================

If change of baudrate is requred, give permission for 'csh' to access network configuration in order to setup the CAN interface. When the file is deleted (rebuild) setcap needs to be run again

```
sudo setcap cap_net_raw,cap_net_admin+ep ./builddir/csh
```

Windows support
===============
CSH does not have native support for running in Windows, but it is possible to use Microsoft WSL, to enable support for Windows based machines.

In a powershell, run the following command
```
wsl --install
```
After a reboot, you can then start the application WSL to get a virtual Ubuntu environment and follow the guidelines for installing CSH in Linux as above. Windows by default does not forward USB devices to WSL entities. To enable USB forwarding, follow the guide in https://learn.microsoft.com/en-us/windows/wsl/connect-usb.