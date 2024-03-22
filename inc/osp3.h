/**
 * A library for managing an ODROID Smart Power 3 (OSP3) device.
 *
 * Supported baud rates: 9600, 19200, 38400, 57600, 115200 (default), 230400, 460800, 500000, 576000, and 921600.
 * However, not all platforms support configuring all baud rates, e.g., macOS is limited.
 *
 * The OSP3 device must first be configured to perform serial logging.
 * Supported logging intervals (some constrained by the baud rate): 5, 10 (default), 50, 100, 500, and 1000 ms.
 *
 * Log entries may be read from the device at the logging interval configured on the device.
 * The max serial packet size is less than that of a log entry, so multiple reads are necessary to capture an entry.
 * The user is responsible for identifying and handling log entry slices.
 *
 * @author Connor Imes
 * @date 2024-03-19
 */
#ifndef _OSP3_H_
#define _OSP3_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * Minimum supported serial baud rate.
 */
#define OSP3_BAUD_MIN 9600

/**
 * Maximum supported serial baud rate.
 */
#define OSP3_BAUD_MAX 921600

/*
 * Default serial baud rate used on the device UI.
 */
#define OSP3_BAUD_DEFAULT 115200

/**
 * Minimum serial logging interval.
 */
#define OSP3_INTERVAL_MS_MIN 5

/**
 * Maximum serial logging interval.
 */
#define OSP3_INTERVAL_MS_MAX 1000

/**
 * Default serial logging interval used on the device UI.
 */
#define OSP3_INTERVAL_MS_DEFAULT 10

/**
 * Maximum serial packet size.
 */
#define OSP3_W_MAX_PACKET_SIZE 64

/**
 * Device log entry line length, including trailing escape characters.
 */
#define OSP3_LOG_PROTOCOL_SIZE 81

/*
 * Interrupt Bits.
 *
 * Bit | Function                         | Notes
 * -----------------------------------------------------------------------------
 * 0   | Overvoltage protection
 * 1   | Constant current function
 * 2   | Short-circuit protection
 * 3   | Power-on
 * 4   | Watchdog
 * 5   | Overtemperature protection       | Junction temperatrue 165 Celsius
 * 6   | Overtemperature warning          | Junction temperature 145 Celsius
 * 7   | Inductor peak current protection
 */
#define OSP3_INTR_OVERVOLTAGE_PROT           (1u)
#define OSP3_INTR_CONSTANT_CURRENT_FUNC      (1u << 1)
#define OSP3_INTR_SHORT_CIRCUIT_PROT         (1u << 2)
#define OSP3_INTR_POWER_ON                   (1u << 3)
#define OSP3_INTR_WATCHDOG                   (1u << 4)
#define OSP3_INTR_OVERTEMPERATURE_PROT       (1u << 5)
#define OSP3_INTR_OVERTEMPERATURE_WARN       (1u << 6)
#define OSP3_INTR_INDUCTOR_PEAK_CURRENT_PROT (1u << 7)

/**
 * Opaque OSP3 handle.
 */
typedef struct osp3_device osp3_device;

typedef struct osp3_log_entry {
  unsigned long ms;
  unsigned int mV_in;
  unsigned int mA_in;
  unsigned int mW_in;
  unsigned int onoff_in;
  unsigned int mV_0;
  unsigned int mA_0;
  unsigned int mW_0;
  unsigned int onoff_0;
  unsigned int intr_0;
  unsigned int mV_1;
  unsigned int mA_1;
  unsigned int mW_1;
  unsigned int onoff_1;
  unsigned int intr_1;
  uint8_t checksum8_2s_compl;
  uint8_t checksum8_xor;
} osp3_log_entry;

/**
 * Open an OSP3 device.
 *
 * @param path The device path
 * @param baud The baud rate (or 0 for default)
 * @return A osp3_device handle, or NULL on failure
 */
osp3_device* osp3_open_device(const char* path, unsigned int baud);

/**
 * Close an OSP3 device handle.
 *
 * @param dev An open device
 * @return 0 in success, -1 on error
 */
int osp3_close(osp3_device* dev);

/**
 * Flush unread data from the receive buffer (e.g., to drop old log entries).
 *
 * @param dev An open device
 * @return 0 on success, -1 on error
 */
int osp3_flush(osp3_device* dev);

/**
 * Read from an OSP3.
 *
 * Any buffered data will be read first.
 * On error, it's possible that some bytes may have been read, but may or may not be reported in `transferred`.
 *
 * @param dev An open device
 * @param buf The destination buffer
 * @param len The destination buffer size (should probably be `OSP3_W_MAX_PACKET_SIZE`)
 * @param transferred The number of bytes actually read
 * @param timeout_ms A timeout in milliseconds
 * @return 0 on success, -1 on error
 */
int osp3_read(osp3_device* dev, unsigned char* buf, size_t len, size_t* transferred, unsigned int timeout_ms);

/**
 * Read a complete line from an OSP3.
 *
 * Multiple reads are often necessary to read a complete line, and the same timeout is used for each read operation.
 * Therefore, this function could theoretically block for up to `len * timeout_ms` milliseconds.
 * However, in practice, OSP3 devices stream entire lines to the serial port, not single bytes in isolation.
 * The total number of reads is thus more likely to be `ceil(line_length / OSP3_W_MAX_PACKET_SIZE)`.
 * After waiting (up to `timeout_ms`) for data to become available for the first read, each additional read likely only
 * blocks long enough for the next packet to be received, repeated until a newline character is found.
 *
 * If the final read captures data after a newline character, it is buffered separately.
 * Any following read (including from `osp3_read`) will first get data from this buffer before reading from the OSP3.
 *
 * @param dev An open device
 * @param buf The destination buffer
 * @param len The destination buffer size
 * @param line_writte The number of bytes actually read
 * @param timeout_ms A timeout in milliseconds
 * @return 0 on success, -1 on error
 */
int osp3_read_line(osp3_device* dev, unsigned char* buf, size_t len, size_t* transferred, unsigned int timeout_ms);

/**
 * Perform a checksum on a log entry.
 *
 * @param log The log entry buffer
 * @param log_sz Must be `>= OSP3_LOG_PROTOCOL_SIZE`
 * @param cs8_2s The resulting 2s complement checksum
 * @param cs8_xor The resulting XOR checksum
 * @return 0 on checksum match, 1 on checksum mismatch, -1 on error
 */
int osp3_log_checksum(const char* log, size_t log_sz, uint8_t* cs8_2s, uint8_t* cs8_xor);

/**
 * Test the given checksum values against a log entry.
 *
 * @param log The log entry buffer
 * @param log_sz Must be `>= OSP3_LOG_PROTOCOL_SIZE`
 * @param cs8_2s The 2s complement checksum
 * @param cs8_xor The XOR checksum
 * @return 0 on checksum match, 1 on checksum mismatch, -1 on error
 */
int osp3_log_checksum_test(const char* log, size_t log_sz, uint8_t cs8_2s, uint8_t cs8_xor);

/**
 * Parse a log entry.
 *
 * @param log The log entry buffer
 * @param log_sz Must be `>= OSP3_LOG_PROTOCOL_SIZE`
 * @param log_entry The struct to be populated
 * @return 0 on success, 1 if not all fields were parsed, -1 on error
 */
int osp3_log_parse(const char* log, size_t log_sz, osp3_log_entry* log_entry);

#ifdef __cplusplus
}
#endif

#endif
