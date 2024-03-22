# ODROID Smart Power 3 Library and Utilities

A library and tools for an [ODROID Smart Power 3](https://wiki.odroid.com/accessory/power_supply_battery/smartpower3) device with a USB connection.

> NOTE: If you're using a first generation [ODROID Smart Power](https://wiki.odroid.com/old_product/accessory/odroidsmartpower) device, see the [hosp](https://github.com/energymon/hosp) project instead.

This project is tested using a device running SmartPower3 firmware v2.2 (20230518), though may work with devices running v1.7 (20211214) or newer.
Older firmware versions use a different logging protocol.


## Building

### Prerequisites

This project uses [CMake](https://cmake.org/).

On Debian-based Linux systems (including Ubuntu):

```sh
sudo apt install cmake
```

On macOS, using [Homebrew](https://brew.sh/):

```sh
brew install cmake
```

### Compiling

To build, run:

```sh
cmake -S . -B build/
cmake --build build/
```

To build a shared object library (instead of a static library), add `-DBUILD_SHARED_LIBS=On` to the first cmake command.
Add `-DCMAKE_BUILD_TYPE=Release` for an optimized build.
Refer to CMake documentation for more a complete description of build options.

To install, run with proper privileges:

```sh
cmake --build . --target install
```

On Linux, installation typically places libraries in `/usr/local/lib` and header files in `/usr/local/include/osp3`.

Install must be run before uninstalling in order to have a manifest.
To uninstall, run with proper privileges (install must have been run first to create a manifest):

```sh
cmake --build . --target uninstall
```

### Linking

To link against `osp3`, use `pkg-config` to get compiler and linker flags.
E.g., in a Makefile:

```sh
CFLAGS+=$(shell pkg-config --cflags osp3)
LDFLAGS+=$(shell pkg-config --libs --static osp3)
```

The `--static` flag is unnecessary if you built/installed a shared object library.


## Linux Privileges

To use an ODROID Smart Power 3 without needing sudo/root at runtime, set appropriate [udev](https://en.wikipedia.org/wiki/Udev) privileges.

You can give access to a specific group, e.g. `plugdev`, by creating/modifying a `udev` config file like `/etc/udev/rules.d/99-osp3.rules`.
For example, add the following rules:

```
# OROID Smart Power 3
SUBSYSTEMS=="usb", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", GROUP="plugdev"
```

For the new permissions to take effect, the device must be remounted by the kernel - either disconnect and reconnect the device or reboot the system.


## Utilities

The following command-line utilities are included.
See their help output for usage.

* `osp3-dump` - dump the device's serial output.
* `osp3-poll` - poll the device's serial output for complete log entries.


## C API

The following is a simple example that reads data from an ODROID Smart Power 3 device.
A real application will likely do more complex things with the data (like parse it).

```C
#include <stdio.h>
#include <stdlib.h>
#include <osp3.h>

int main(void) {
  int ret = 0;

  // Open the device.
  const char* path = "/dev/ttyUSB0";
  unsigned int baud = OSP3_BAUD_DEFAULT;
  osp3_device* dev;
  if ((dev = osp3_open_device(path, baud)) == NULL) {
    perror("Failed to open ODROID Smart Power 3 connection");
    return 1;
  }

  // Work with the device.
  unsigned char packet[OSP3_W_MAX_PACKET_SIZE] = { 0 };
  size_t transferred = 0;
  unsigned int timeout_ms = 0;
  while (1) {
    if (osp3_read(dev, packet, sizeof(packet), &transferred, timeout_ms) < 0) {
      perror("Failed to read from ODROID Smart Power 3");
      ret = 1;
      break;
    }
    for (size_t i = 0; i < transferred; i++) {
      putchar((const char) packet[i]);
    }
  }

  // Close the device.
  if (osp3_close(dev)) {
    perror("Failed to close ODROID Smart Power 3 connection");
    ret = 1;
  }

  return ret;
}
```


## Project Source

Find this and related project sources at the [energymon organization on GitHub](https://github.com/energymon).  
This project originates at: https://github.com/energymon/osp3

Bug reports and pull requests for bug fixes and enhancements are welcome.
