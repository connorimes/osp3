/**
 * OSP3 internal interface.
 *
 * This API is primarily to abstract away the OS-level device handling.
 *
 * @author Connor Imes
 * @date 2024-04-08
 */
#ifndef _OSP3I_
#define _OSP3I_

#include <sys/types.h>
#include <osp3.h>

#pragma GCC visibility push(hidden)

typedef struct osp3_rw_buffer {
  unsigned char buf[OSP3_W_MAX_PACKET_SIZE];
  size_t idx;
  size_t rem;
} osp3_rw_buffer;

struct osp3_device {
  osp3_rw_buffer rbuf;
  int fd;
};

int osp3i_open_path(osp3_device* dev, const char* filename, unsigned int baud);

int osp3i_close(osp3_device* dev);

int osp3i_flush(osp3_device* dev);

ssize_t osp3i_read(osp3_device* dev, unsigned char* buf, size_t buflen, unsigned int timeout_ms);

/**
 * Darwin (macOS) doesn't support all the necessary POSIX baud rates, so it uses a different implementation.
 */
int osp3i_serial_configure(osp3_device* dev, unsigned int baud);

#pragma GCC visibility pop

#endif
