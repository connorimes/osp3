/**
 * OSP3 internal interface common functions (POSIX).
 *
 * @author Connor Imes
 * @date 2024-04-08
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <osp3.h>
#include "osp3i.h"

int osp3i_open_path(osp3_device* dev, const char* filename, unsigned int baud) {
  struct stat s;
  if (stat(filename, &s) < 0) {
    return -1;
  }
  if (!S_ISCHR(s.st_mode)) {
    errno = ENOTTY;
    return -1;
  }
  if ((dev->fd = open(filename, O_RDONLY | O_NONBLOCK)) < 0) {
    return -1;
  }
  if (osp3i_serial_configure(dev, baud) < 0) {
    close(dev->fd);
    return -1;
  }
  return 0;
}

int osp3i_close(osp3_device* dev) {
  return close(dev->fd);
}

int osp3i_flush(osp3_device* dev) {
  return tcflush(dev->fd, TCIFLUSH);
}

ssize_t osp3i_read(osp3_device* dev, unsigned char* buf, size_t buflen, unsigned int timeout_ms) {
  ssize_t ret = -1;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(dev->fd, &set);
  struct timespec ts_timeout = {
    .tv_sec = timeout_ms / 1000,
    .tv_nsec = (timeout_ms % 1000) * 1000 * 1000,
  };
  switch (pselect(dev->fd + 1, &set, NULL, NULL, timeout_ms > 0 ? &ts_timeout : NULL, NULL)) {
    case -1:
      // failed
      break;
    case 0:
      // timed out
      errno = ETIME;
      break;
    default:
      ret = read(dev->fd, buf, buflen);
      break;
  }
  return ret;
}
