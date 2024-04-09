/**
 * Poll log entries from an ODROID Smart Power 3.
 *
 * @author Connor Imes
 * @date 2024-03-19
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <osp3.h>

#define PATH_DEFAULT "/dev/ttyUSB0"

// Conservative, but effective.
#define TIMEOUT_MS_DEFAULT (OSP3_INTERVAL_MS_MAX * 2)

// Much bigger than anything an OSP3 should produce.
#define OSP3_LINE_LEN_MAX 1024

static int path_set = 0;
static const char* path = PATH_DEFAULT;
static unsigned int baud = OSP3_BAUD_DEFAULT;
static unsigned int timeout_ms = TIMEOUT_MS_DEFAULT;
static volatile int running = 1;
static int count = 0;
static int parse = 1;
static int checksum = 1;

static const char short_options[] = "hp::b:t:n:";
static const struct option long_options[] = {
  {"help",        no_argument,       NULL, 'h'},
  {"path",        optional_argument, NULL, 'p'},
  {"baud",        required_argument, NULL, 'b'},
  {"timeout",     required_argument, NULL, 't'},
  {"num",         required_argument, NULL, 'n'},
  // Long-only options.
  {"no-parse",    no_argument,       &parse, 0},
  {"no-checksum", no_argument,       &checksum, 0},
  {0, 0, 0, 0}
};

__attribute__ ((noreturn))
static void print_usage(int exit_code) {
  fprintf(exit_code ? stderr : stdout,
          "Poll log entries from an ODROID Smart Power 3.\n\n"
          "Usage: osp3-poll [OPTION]...\n"
          "Options:\n"
          "  -h, --help               Print this message and exit\n"
          "  -p, --path=FILE          Device path (default: %s);\n"
          "                           No FILE, \"\", or \"-\" uses standard input\n"
          "  -b, --baud=RATE          Device baud rate (default: %u)\n"
          "  -t, --timeout=MS         Read timeout in milliseconds (default: %u)\n"
          "                           Use 0 for blocking read\n"
          "  -n, --num=N              Stop after N log entries\n"
          "  --no-parse               Disable log entry parsing verification\n"
          "  --no-checksum            Disable log entry checksum verification\n",
          PATH_DEFAULT, OSP3_BAUD_DEFAULT, TIMEOUT_MS_DEFAULT);
  exit(exit_code);
}

static void parse_args(int argc, char** argv) {
  int c;
  while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (c) {
      case 'h':
        print_usage(0);
        break;
      case 'p':
        path_set = 1;
        path = optarg;
        break;
      case 'b':
        baud = (unsigned int) atoi(optarg);
        break;
      case 't':
        timeout_ms = (unsigned int) atoi(optarg);
        break;
      case 'n':
        count = 1;
        running = atoi(optarg);
        break;
      case 0:
        // Long-only option.
        break;
      case '?':
      default:
        print_usage(1);
        break;
    }
  }
}

static void shandle(int sig) {
  switch (sig) {
    case SIGTERM:
    case SIGINT:
#ifdef SIGQUIT
    case SIGQUIT:
#endif
#ifdef SIGKILL
    case SIGKILL:
#endif
#ifdef SIGHUP
    case SIGHUP:
#endif
      running = 0;
    default:
      break;
  }
}

static int stdin_wait(void) {
  static const int fd = 0; // stdin
  int ret = 0;
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
      ret = -1;
      break;
    case 0:
      // timed out
      errno = ETIME;
      ret = -1;
      break;
    default:
      ret = 0;
      break;
  }
  return ret;
}

static int stdin_read_line(unsigned char* buf, size_t len, size_t* transferred) {
  *transferred = 0;
  while (running) {
    if (stdin_wait() < 0) {
      return -1;
    }
    int c;
    if ((c = getchar()) == EOF) {
      break;
    }
    buf[(*transferred)] = (unsigned char) c;
    (*transferred)++;
    if (c == '\n') {
      return 0;
    }
    if (*transferred == len) {
      errno = ENOBUFS;
      return -1;
    }
  }
  running = 0;
  return -1;
}

static int read_line(osp3_device* dev, unsigned char* buf, size_t len, size_t* transferred) {
  return dev == NULL ? stdin_read_line(buf, len, transferred) : osp3_read_line(dev, buf, len, transferred, timeout_ms);
}

static int osp3_poll(osp3_device* dev) {
  // Print header.
  printf("ms,");
  printf("mV_in,mA_in,mW_in,onoff_in,");
  printf("mV_0,mA_0,mW_0,onoff_0,interrupts_0,");
  printf("mV_1,mA_1,mW_1,onoff_1,interrupts_1,");
  printf("CheckSum8_2s_Complement,CheckSum8_Xor\n");
  osp3_log_entry log_entry;
  uint8_t cs8_2s;
  uint8_t cs8_xor;
  while (running) {
    char line[OSP3_LINE_LEN_MAX + 1] = { 0 };
    size_t line_written = 0;
    if (read_line(dev, (unsigned char*) line, sizeof(line) - 1, &line_written) < 0) {
      if (!running) {
        return 0;
      }
      if (errno == ETIME) {
        fprintf(stderr, "Read timeout expired\n");
      } else {
        perror("read_line");
      }
      return 1;
    }
    assert(line_written > 0);
    assert(line[line_written - 1] == '\n');
    // If the line came from the serial port, we should expect `line_written == OSP3_LOG_PROTOCOL_SIZE`.
    // However, a line from stdin may not include the '\r' prior to the '\n', so we'll try to be forgiving.
    // Parsing and checksum should still drop bad messages (unless disabled, but that's the user being reckless).
    if (parse && line_written < OSP3_LOG_PROTOCOL_SIZE - 1) {
      fprintf(stderr, "Log entry parsing failed (too short): %s", line);
    } else if (parse && line_written > OSP3_LOG_PROTOCOL_SIZE) {
      fprintf(stderr, "Log entry parsing failed (too long): %s", line);
    } else if (parse && osp3_log_parse(line, OSP3_LOG_PROTOCOL_SIZE, &log_entry)) {
      fprintf(stderr, "Log entry parsing failed (bad format): %s", line);
    } else if (checksum && osp3_log_checksum(line, OSP3_LOG_PROTOCOL_SIZE, &cs8_2s, &cs8_xor)) {
      fprintf(stderr, "Log entry checksum failed (cs8_2s=%02x, cs8_xor=%02x): %s", cs8_2s, cs8_xor, line);
    } else {
      printf("%s", line);
      if (count) {
        running--;
      }
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  osp3_device* dev = NULL;
  int ret;

  // Flushing lines improves streaming performance when stdout is non-interactive, e.g., piped to another process.
  // This enables better (soft) real-time pipeline processing.
  setlinebuf(stdout);

  parse_args(argc, argv);

  if (path_set || (isatty(0) && path != NULL && strlen(path) > 0 && strcmp(path, "-"))) {
    signal(SIGINT, shandle);
    if ((dev = osp3_open_path(path, baud)) == NULL) {
      perror("Failed to open ODROID Smart Power 3 connection");
      return 1;
    }
  }

  ret = osp3_poll(dev);

  if (dev != NULL && osp3_close(dev)) {
    perror("Failed to close ODROID Smart Power 3 connection");
  }

  return ret;
}
