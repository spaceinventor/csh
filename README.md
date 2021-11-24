### spaceinventor/satctl changes name to spaceinventor/spacepromt

`satctl` was originally a piece of software under license from 'satlab ApS'. Since the current
version of 'spaceinventor/satctl' has nothing to do with satlab or the original `satctl`
we are changing the name of both the software and the repository to avoid any misunderstanding.


The process will be gradual to avoid too many broken links so the old repo will remain in place for a while still.
But the software will be called `spacepromt` from now on.


### Build

Requirements: build-essential, libsocketcan-dev, libzmq-dev, meson

```
sudo apt-get install libsocketcan-dev can-utils
sudo pip3 install meson ninja
```

Sometimes needed:
```
link /usr/sbin/ninja /usr/local/lib/python3.5/dist-packages/ninja
```

Build
```
git clone git@github.com:spaceinventor/satctl.git
cd spacepromt
git submodule update --init --recursive

meson . builddir
cd builddir
ninja
```

### Run

To setup CAN sudo is required:

sudo ./caninit.sh

On subsequent launches, sudo is not needed.

Then launch two instances
`./spacepromt -n1 -c vcan42`
And
`./spacepromt -n2 -c vcan42`

Then on instance 1 type `ping 2`
