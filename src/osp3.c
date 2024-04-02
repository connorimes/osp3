/**
 * A library for managing an ODROID Smart Power 3 device over USB.
 *
 * @author Connor Imes
 * @date 2024-03-19
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <osp3.h>

#ifndef OSP3_DEBUG
  #define OSP3_DEBUG 0
#endif

typedef struct osp3_rw_buffer {
  unsigned char buf[OSP3_W_MAX_PACKET_SIZE];
  size_t idx;
  size_t rem;
} osp3_rw_buffer;

struct osp3_device {
  osp3_rw_buffer rbuf;
  int fd;
};

static speed_t baud_to_speed(unsigned int baud) {
  switch (baud) {
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  case 230400:
    return B230400;
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

static int osp3_set_serial_attributes(int fd, unsigned int baud) {
  struct termios t;
  speed_t speed;
  if (tcgetattr(fd, &t) < 0) {
    return -1;
  }
  cfmakeraw(&t);
  if ((speed = baud_to_speed(baud)) == B0) {
#if OSP3_DEBUG
    fprintf(stderr, "Unsupported baud: %u\n", baud);
#endif
    return -1;
  }
  if (cfsetspeed(&t, speed) < 0) {
    return -1;
  }
  if (tcsetattr(fd, TCSANOW, &t) < 0) {
    return -1;
  }
  return tcflush(fd, TCIFLUSH);
}

static int osp3_open(const char* filename, int* fd) {
  struct stat s;
  char buf[32];
  const char* shortname;

  // Check if device node exists and is writable
  if (stat(filename, &s) < 0) {
#if OSP3_DEBUG
    perror(filename);
#endif
    return -1;
  }
  if (!S_ISCHR(s.st_mode)) {
    errno = ENOTTY;
#if OSP3_DEBUG
    perror("osp3_open: Not a TTY character device");
#endif
    return -1;
  }
  if (access(filename, R_OK | W_OK)) {
    perror(filename);
    return -1;
  }

  // Get shortname by dropping leading "/dev/"
  if (!(shortname = strrchr(filename, '/'))) {
    // shouldn't happen since we've already checked filename
    errno = EINVAL;
#if OSP3_DEBUG
    perror(filename);
#endif
    return -1;
  }
  shortname++;

  // Check if "/sys/class/tty/<shortname>" exists and is correct type
  snprintf(buf, sizeof(buf), "/sys/class/tty/%s", shortname);
  if (stat(buf, &s) < 0) {
#if OSP3_DEBUG
    perror(buf);
#endif
    return -1;
  }
  if (!S_ISDIR(s.st_mode)) {
    errno = ENODEV;
#if OSP3_DEBUG
    perror("osp3_open: Not a TTY device");
#endif
    return -1;
  }

  // Open the device file
  *fd = open(filename, O_RDONLY | O_NONBLOCK);
  if (*fd < 0) {
#if OSP3_DEBUG
    perror(filename);
#endif
    return -1;
  }
  return 0;
}

osp3_device* osp3_open_device(const char* path, unsigned int baud) {
  osp3_device* dev;
  if (path == NULL) {
    errno = EINVAL;
    return NULL;
  }
  if ((dev = calloc(1, sizeof(osp3_device))) == NULL) {
    return NULL;
  }
  if (osp3_open(path, &dev->fd) < 0) {
    free(dev);
    return NULL;
  }
  if (osp3_set_serial_attributes(dev->fd, baud > 0 ? baud : OSP3_BAUD_DEFAULT) < 0) {
    close(dev->fd);
    free(dev);
    return NULL;
  }
  return dev;
}

int osp3_close(osp3_device* dev) {
  int ret = close(dev->fd);
  free(dev);
  return ret;
}

int osp3_flush(osp3_device* dev) {
  dev->rbuf.idx = 0;
  dev->rbuf.rem = 0;
  return tcflush(dev->fd, TCIFLUSH);
}

static ssize_t osp3_read_buf(int fd, unsigned char* buf, size_t buflen, unsigned int timeout_ms) {
  ssize_t ret = -1;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  struct timespec ts_timeout = {
    .tv_sec = timeout_ms / 1000,
    .tv_nsec = (timeout_ms % 1000) * 1000 * 1000,
  };
  switch (pselect(fd + 1, &set, NULL, NULL, timeout_ms > 0 ? &ts_timeout : NULL, NULL)) {
    case -1:
      // failed
      break;
    case 0:
      // timed out
      errno = ETIME;
      break;
    default:
      ret = read(fd, buf, buflen);
      break;
  }
  return ret;
}

static size_t sz_min(size_t a, size_t b) {
  return a <= b ? a : b;
}

int osp3_read(osp3_device* dev, unsigned char* buf, size_t len, size_t* transferred, unsigned int timeout_ms) {
  *transferred = sz_min(dev->rbuf.rem, len);
  memcpy(buf, &dev->rbuf.buf[dev->rbuf.idx], *transferred);
  assert(dev->rbuf.rem >= *transferred);
  dev->rbuf.rem -= *transferred;
  dev->rbuf.idx = dev->rbuf.rem > 0 ? dev->rbuf.idx + *transferred : 0;
  if (*transferred < len) {
    ssize_t bytes_read = osp3_read_buf(dev->fd, &buf[*transferred], len - *transferred, timeout_ms);
    if (bytes_read < 0) {
      return -1;
    }
    *transferred += (size_t) bytes_read;
  }
  return 0;
}

static int lineccpy(void* restrict dst, size_t dst_sz, size_t* written, const void* restrict src, size_t src_sz) {
  const size_t sz = sz_min(dst_sz, src_sz);
  const void* ret = memccpy(dst, src, '\n', sz);
  *written = ret == NULL ? sz : (size_t) ((const char*) ret - (const char*) dst);
  return ret == NULL ? 0 : 1;
}

int osp3_read_line(osp3_device* dev, unsigned char* buf, size_t len, size_t* transferred, unsigned int timeout_ms) {
  size_t line_seg_written = 0;
  int complete = lineccpy(buf, len, &line_seg_written, &dev->rbuf.buf[dev->rbuf.idx], dev->rbuf.rem);
  *transferred = line_seg_written;
  assert(dev->rbuf.rem >= line_seg_written);
  dev->rbuf.rem -= line_seg_written;
  dev->rbuf.idx = dev->rbuf.rem > 0 ? dev->rbuf.idx + line_seg_written : 0;
  while (!complete) {
    unsigned char packet[OSP3_W_MAX_PACKET_SIZE];
    assert(len >= *transferred);
    size_t packet_sz = sz_min(sizeof(packet), len - *transferred);
    if (packet_sz == 0) {
      errno = ENOBUFS;
      return -1;
    }
    ssize_t bytes_read = osp3_read_buf(dev->fd, packet, packet_sz, timeout_ms);
    if (bytes_read < 0) {
      return -1;
    }
    size_t packet_written = (size_t) bytes_read;
    line_seg_written = 0;
    complete = lineccpy(&buf[*transferred], len - *transferred, &line_seg_written, packet, packet_written);
    *transferred += line_seg_written;
    assert(dev->rbuf.idx == 0);
    assert(packet_written >= line_seg_written);
    dev->rbuf.rem = packet_written - line_seg_written;
    assert(dev->rbuf.rem > 0 ? complete : 1); // if we actually populate the buffer, we must be finished
    memcpy(dev->rbuf.buf, &packet[line_seg_written], dev->rbuf.rem);
  }
  return 0;
}

// TOTAL: 81 (79 printable characters + 2 escape characters)
// Time| INPUT POWER                           | CHANNEL 0                                         | CHANNEL 1                                         | CHECKSUM                              | LF
// (ms), volt(mV), ampere(mA), watt(mW), on/off, volt(mV), ampere(mA), watt(mW), on/off, interrupts, volt(mV), ampere(mA), watt(mW), on/off, interrupts, CheckSum8 2s Complement, CheckSum8 Xor '\r\n'
// 0000815169,15296,0036,00550,0,00000,0000,00000,0,00,00000,0000,00000,0,00,14,12\r\n
#define MS_SZ 10
#define MV_SZ 5
#define MA_SZ 4
#define MW_SZ 5
#define ONOFF_SZ 1
#define INTR_SZ 2
#define CS_2COMPL_SZ 2
#define CS_XOR_SZ 2

#define MS_OFF 0

#define MV_IN_OFF (MS_OFF + MS_SZ + 1)
#define MA_IN_OFF (MV_IN_OFF + MV_SZ + 1)
#define MW_IN_OFF (MA_IN_OFF + MA_SZ + 1)
#define ONOFF_IN_OFF (MW_IN_OFF + MW_SZ + 1)

#define MV_0_OFF (ONOFF_IN_OFF + ONOFF_SZ + 1)
#define MA_0_OFF (MV_0_OFF + MV_SZ + 1)
#define MW_0_OFF (MA_0_OFF + MA_SZ + 1)
#define ONOFF_0_OFF (MW_0_OFF + MW_SZ + 1)
#define INTR_0_OFF (ONOFF_0_OFF + ONOFF_SZ + 1)

#define MV_1_OFF (INTR_0_OFF + INTR_SZ + 1)
#define MA_1_OFF (MV_1_OFF + MV_SZ + 1)
#define MW_1_OFF (MA_1_OFF + MA_SZ + 1)
#define ONOFF_1_OFF (MW_1_OFF + MW_SZ + 1)
#define INTR_1_OFF (ONOFF_1_OFF + ONOFF_SZ + 1)

#define CS_2COMPL_OFF (INTR_1_OFF + INTR_SZ + 1)
#define CS_XOR_OFF (CS_2COMPL_OFF + CS_2COMPL_SZ + 1)

static void osp3_log_checksum_compute(const char* log, uint8_t* cs8_2s, uint8_t* cs8_xor) {
  *cs8_2s = 0;
  *cs8_xor = 0;
  for (size_t i = 0; i < CS_2COMPL_OFF; i++) {
    *cs8_2s += (unsigned char) log[i];
    *cs8_xor ^= (unsigned char) log[i];
  }
  *cs8_2s = (~(*cs8_2s)) + 1;
}

int osp3_log_checksum(const char* log, size_t log_sz, uint8_t* cs8_2s, uint8_t* cs8_xor) {
  if (log_sz < OSP3_LOG_PROTOCOL_SIZE) {
    errno = EINVAL;
    return -1;
  }
  osp3_log_checksum_compute(log, cs8_2s, cs8_xor);
  return osp3_log_checksum_test(log, log_sz, *cs8_2s, *cs8_xor);
}

int osp3_log_checksum_test(const char* log, size_t log_sz, uint8_t cs8_2s, uint8_t cs8_xor) {
  if (log_sz < OSP3_LOG_PROTOCOL_SIZE) {
    errno = EINVAL;
    return -1;
  }
  // Log values are in hexadecimal.
  // Arrays are at least 8 bytes long to avoid `stack-protector` warnings.
  char cs8_2s_log_bytes[8] = { log[CS_2COMPL_OFF], log[CS_2COMPL_OFF + 1], '\0' };
  char cs8_xor_log_bytes[8] = { log[CS_XOR_OFF], log[CS_XOR_OFF + 1], '\0' };
  uint8_t cs8_2s_log = (uint8_t) strtoul(cs8_2s_log_bytes, NULL, 16);
  uint8_t cs8_xor_log = (uint8_t) strtoul(cs8_xor_log_bytes, NULL, 16);
  return !(cs8_2s == cs8_2s_log && cs8_xor == cs8_xor_log);
}

int osp3_log_parse(const char* log, size_t log_sz, osp3_log_entry* log_entry) {
  if (log_sz < OSP3_LOG_PROTOCOL_SIZE) {
    errno = EINVAL;
    return -1;
  }
  int matched = sscanf(log,
    "%010lu,%05u,%04u,%05u,%1u,%05u,%04u,%05u,%1u,%02x,%05u,%04u,%05u,%1u,%02x,%"SCNx8",%"SCNx8"\r\n",
    // Time
    &log_entry->ms,
    // Input Power
    &log_entry->mV_in, &log_entry->mA_in, &log_entry->mW_in, &log_entry->onoff_in,
    // Channel 0 Output
    &log_entry->mV_0, &log_entry->mA_0, &log_entry->mW_0, &log_entry->onoff_0, &log_entry->intr_0,
    // Channel 1 Output
    &log_entry->mV_1, &log_entry->mA_1, &log_entry->mW_1, &log_entry->onoff_1, &log_entry->intr_1,
    // Checksum
    &log_entry->checksum8_2s_compl, &log_entry->checksum8_xor);
  if (matched == EOF) {
    if (!errno) {
      errno = EILSEQ;
    }
    return -1;
  }
  return !(matched == 17);
}
