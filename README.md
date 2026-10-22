# CSP Shell

CSP Shell or `csh` pronounced *seashell* is a command line interpreter for running CSP and `libparam` based applications.

## spaceinventor/satctl changes name to spaceinventor/csh

`satctl` was originally a piece of software under license from 'Satlab A/S'. Since the current
version of 'spaceinventor/satctl' has nothing to do with satlab or the original `satctl`
we are changing the name of both the software and the repository to avoid any misunderstanding.

The software is now called `csh` (pronounced 'seashell').

## Installing

Download and install a pre-built Ubuntu Packages from Github's [Release Pages](https://github.com/spaceinventor/csh/releases).

## Running

To start CSH, execute the command `csh` to start a new instance, loading the default init file (~/init.csh) if it exists.

Starting CSH explicitely without loading an init file is done by  `csh -i`.
Same argument can be used to select a custom init file when starting the application.

### Access to manuals

Once started, the built-in manuals can be opened by using the command `manual <tab>` which will show available manuals installed.

## Build locally

![build.yml status](https://github.com/spaceinventor/csh/actions/workflows/build.yml/badge.svg)

In case the latest release do not include required functionality, or for development of CSH itself, the application can be built locally.

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

### Running local build

When running a build not installed using `apt`, CSH needs access to the CAN driver by executing

```
sudo caninit
```

CSH is then started in a similar way as when using an installed version.

## Windows support

CSH does not have native support for running in Windows, but it is possible to use Microsoft WSL, to enable support for Windows based machines.

In a powershell, run the following command
```
wsl --install
```
After a reboot, you can then start the application WSL to get a virtual Ubuntu environment and follow the guidelines for installing CSH in Linux as above. Please notice that CAN dongles are not natively supported in the Ubuntu kernel for WSL. Connecting to a ZMQ network is done in the same way as in native Ubuntu.

## Extension support

CSH allows a user to execute and build extensions to the applications known as APMs. To load the available APMs available on a machine, execute the command 
```
apm load
```

When using CSH installed using as a pre-built package, the application comes with APMs designed to operate Space Inventor PDUs, OBCs and CPSS (Centralized Payload Storage Service).

New custom APMs are build on basis of the repository https://github.com/spaceinventor/csh_example.
