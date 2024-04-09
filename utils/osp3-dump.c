/**
 * Dump serial output from an ODROID Smart Power 3.
 *
 * @author Connor Imes
 * @date 2024-03-27
 */
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <osp3.h>

#define PATH_DEFAULT "/dev/ttyUSB0"

#define TIMEOUT_MS_DEFAULT 0

static const char* path = PATH_DEFAULT;
static unsigned int baud = OSP3_BAUD_DEFAULT;
static unsigned int timeout_ms = TIMEOUT_MS_DEFAULT;
static volatile int running = 1;

static const char short_options[] = "hp:b:t:";
static const struct option long_options[] = {
  {"help",      no_argument,       NULL, 'h'},
  {"path",      required_argument, NULL, 'p'},
  {"baud",      required_argument, NULL, 'b'},
  {"timeout",   required_argument, NULL, 't'},
  {0, 0, 0, 0}
};

__attribute__ ((noreturn))
static void print_usage(int exit_code) {
  fprintf(exit_code ? stderr : stdout,
          "Dump serial output from an ODROID Smart Power 3.\n\n"
          "Usage: osp3-dump [OPTION]...\n"
          "Options:\n"
          "  -h, --help               Print this message and exit\n"
          "  -p, --path=FILE          Device path (default: %s)\n"
          "  -b, --baud=RATE          Device baud rate (default: %u)\n"
          "  -t, --timeout=MS         Read timeout in milliseconds (default: %u)\n"
          "                           Use 0 for blocking read\n",
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
        path = optarg;
        break;
      case 'b':
        baud = (unsigned int) atoi(optarg);
        break;
      case 't':
        timeout_ms = (unsigned int) atoi(optarg);
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

static int osp3_dump(osp3_device* dev) {
  unsigned char packet[OSP3_W_MAX_PACKET_SIZE] = { 0 };
  while (running) {
    size_t transferred = 0;
    if (osp3_read(dev, packet, sizeof(packet), &transferred, timeout_ms) < 0) {
      if (!running) {
        return 0;
      }
      if (errno == ETIME) {
        fprintf(stderr, "Read timeout expired\n");
      } else {
        perror("osp3_read");
      }
      return 1;
    }
    for (size_t i = 0; i < transferred; i++) {
      putchar((const char) packet[i]);
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  osp3_device* dev;
  int ret;

  // Flushing lines improves streaming performance when stdout is non-interactive, e.g., piped to another process.
  // This enables better (soft) real-time pipeline processing.
  setlinebuf(stdout);

  signal(SIGINT, shandle);
  parse_args(argc, argv);

  if ((dev = osp3_open_path(path, baud)) == NULL) {
    perror("Failed to open ODROID Smart Power 3 connection");
    return 1;
  }

  ret = osp3_dump(dev);

  if (osp3_close(dev)) {
    perror("Failed to close ODROID Smart Power 3 connection");
  }

  return ret;
}
