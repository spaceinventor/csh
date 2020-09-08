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
cd satctl
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
`./satctl -n1 -c vcan42`
And
`./satctl -n2 -c vcan42`

Then on instance 1 type `ping 2`
