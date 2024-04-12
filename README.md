# ODROID Smart Power 3 Library and Utilities

A library and tools for an [ODROID Smart Power 3](https://wiki.odroid.com/accessory/power_supply_battery/smartpower3) device with a USB connection.

> NOTE: If you're using a first generation [ODROID Smart Power](https://wiki.odroid.com/old_product/accessory/odroidsmartpower) device, see the [hosp](https://github.com/energymon/hosp) project instead.

The `osp3` library is designed to support soft real-time power/energy monitoring in applications.
The library handles the USB serial connection and exposes functions to read and parse log lines and verify checksums.

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

If your project uses CMake, find the `OSP3` package and link against its `osp3` library:

```cmake
find_package(OSP3 REQUIRED)
target_link_libraries(foo PRIVATE OSP3::osp3)
```

Otherwise use `pkg-config`, e.g., in a Makefile:

```Makefile
CFLAGS+=$(shell pkg-config --cflags osp3)
LDFLAGS+=$(shell pkg-config --libs --static osp3)

foo:
  $(CC) $(CFLAGS) -o foo foo.c $(LDFLAGS)
```

The `--static` flag is unnecessary if you built/installed a shared object library.


## Linux Privileges

By default, sudo/root privileges are usually needed to access the USB device file, e.g., at `/dev/ttyUSB0`.
To access without requiring sudo/root, add your user to the `dialout` group, e.g.:

```sh
sudo usermod -a -G dialout $USER
```

You will need to logout and log back in for the permission changes to take effect.


## Utilities

The following command-line utilities are included.
See their help output for usage.

* `osp3-dump` - dump the device's serial output.
* `osp3-poll` - poll the device's serial output for complete log entries.

The default timeout in `osp3-poll` exceeds the maximum configurable logging interval so as to be tolerant of any device configuration without blocking indefinitely.
If you have stricter timing considerations, consider decreasing the timeout using the `-t/--timeout` option to more closely match the device's configured logging interval.

While `osp3-poll` reads from the serial port by default, it can also read from standard input.
For example:

```sh
osp3-dump | osp3-poll
````

Or, more usefully, if you have WiFi UDP logging configured:

```sh
netcat -u -l -p 6000 | osp3-poll
```

> HINT: WiFi logging is likely to be both higher latency and less reliable than a wired serial connection.
> If you find that reads are timing out but you can tolerate the latency impact, consider increasing the `osp3-poll` read timeout or set it to 0 to use blocking reads.


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
  if ((dev = osp3_open_path(path, baud)) == NULL) {
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
