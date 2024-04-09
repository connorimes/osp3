/**
 * OSP3 internal interface Darwin (macOS)-only functions.
 *
 * @author Connor Imes
 * @date 2024-04-08
 */
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#include <osp3.h>
#include "osp3i.h"

static speed_t baud_to_speed_darwin(unsigned int baud) {
  switch (baud) {
  case 9600:
  case 19200:
  case 38400:
  case 57600:
  case 115200:
  case 230400:
  case 460800:
  case 500000:
  case 576000:
  case 921600:
    return (speed_t) baud;
  // Higher baud rates not supported by device.
  default:
    break;
  }
  errno = EINVAL;
  return 0;
}

static int set_baud_darwin(int fd, unsigned int baud) {
  speed_t speed;
  // It's cleaner to always use ioctl rather than trying the POSIX approach and then handling unsupported values.
  if ((speed = baud_to_speed_darwin(baud)) == 0 || ioctl(fd, IOSSIOSPEED, &speed) == -1) {
    return -1;
  }
  return 0;
}

int osp3i_serial_configure(osp3_device* dev, unsigned int baud) {
  struct termios t;
  if (tcgetattr(dev->fd, &t) < 0) {
    return -1;
  }
  cfmakeraw(&t);
  if (tcsetattr(dev->fd, TCSANOW, &t) < 0) {
    return -1;
  }
  // It's important that this happen after tcsetattr, otherwise it's overridden.
  if (set_baud_darwin(dev->fd, baud) < 0) {
    return -1;
  }
  return 0;
}
