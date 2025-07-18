# CSP Shell

CSP Shell or `csh` pronounced *seashell* is a command line interpreter for running CSP and `libparam` based applications.

## spaceinventor/satctl changes name to spaceinventor/csh

`satctl` was originally a piece of software under license from 'Satlab A/S'. Since the current
version of 'spaceinventor/satctl' has nothing to do with satlab or the original `satctl`
we are changing the name of both the software and the repository to avoid any misunderstanding.

The process will be gradual to avoid too many broken links so the old repo will remain in place for a while still.
But the software will be called `csh` (pronounced 'seashell') from now on.

## Installing

Preferrably, you should use the provided Ubuntu Packages from Github's [Release Pages](https://github.com/spaceinventor/csh/releases)

## Build

![build.yml status](https://github.com/spaceinventor/csh/actions/workflows/build.yml/badge.svg)

Requirements: libcurl4-openssl-dev build-essential, libsocketcan-dev, can-utils, libzmq3-dev, libyaml-dev, meson, ninja, pkg-config, fonts-powerline, python3-pip, libelf-dev, libbsd-dev

```
sudo apt install libcurl4-openssl-dev git build-essential libsocketcan-dev can-utils libzmq3-dev libyaml-dev pkg-config fonts-powerline pipx libelf-dev libbsd-dev python3-dev
pipx install meson
```

Build:
```
git clone --recurse-submodules https://github.com/spaceinventor/csh.git
cd csh
./configure
./install
```

## Run

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


## Extension support

src/slash_apm.c defines a command, apm load, for loading a shared library as an APM, and a command, ap, info, for listing loaded APMs.

### APM API

An APM optionally defines the following functions

```
libmain(int argc, char ** argv)
```
If defined, this is called once after having loaded the APM. The function is intended to initialize the APM after which it must return, as the CSH command line interface that is calling the function will otherwise hang.

```
libinfo()
```
If defined, this is called by the APM info call. The intention is to provide a means of providing information on the state of the APM, like statistics.

See https://github.com/spaceinventor/csh_example for example usage.

### Features moved to APMs

The following APMs were implemented as a more thorough test of the APM implementation.

`csh_hk`: `hk retrieve` For retrieving historical telemetry stored on an OBC.
`csh_cortex`: `csp add cortex` cortex CSP interface implementation.
`csh_cspftp`: FTP client over CSP.
`cping`: Concurrent, hence faster, ping and scan in one command `cping`.

``Observe that hk retrieve and cortex implementation is removed from plain CSH and must be added by addin load command.``
