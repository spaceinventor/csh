### Build

Requirements: build-essential, libsocketcan-dev, libzmq-dev

./meson . builddir
cd builddir
ninja
sudo ninja install

### Run

To setup CAN sudo is required:

sudo satctl

On subsequent launches, sudo is not needed.

