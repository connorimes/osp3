/**
 * OSP3 internal interface POSIX-only functions.
 *
 * @author Connor Imes
 * @date 2024-04-08
 */
#include <errno.h>
#include <termios.h>
#include <osp3.h>
#include "osp3i.h"

static speed_t baud_to_speed_posix(unsigned int baud) {
  switch (baud) {
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
#ifdef B57600
  case 57600:
    return B57600;
#endif
#ifdef B115200
  case 115200:
    return B115200;
#endif
#ifdef B230400
  case 230400:
    return B230400;
#endif
#ifdef B460800
  case 460800:
    return B460800;
#endif
#ifdef B500000
  case 500000:
    return B500000;
#endif
#ifdef B576000
  case 576000:
    return B576000;
#endif
#ifdef B921600
  case 921600:
    return B921600;
#endif
  // Higher baud rates not supported by device.
  default:
    break;
  }
  errno = EINVAL;
  return B0;
}

static int set_baud_posix(struct termios* t, unsigned int baud) {
  speed_t speed;
  if ((speed = baud_to_speed_posix(baud)) == B0 || cfsetspeed(t, speed) < 0) {
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
  if (set_baud_posix(&t, baud) < 0) {
    return -1;
  }
  if (tcsetattr(dev->fd, TCSANOW, &t) < 0) {
    return -1;
  }
  return 0;
}
