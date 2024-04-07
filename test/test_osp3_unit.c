#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <osp3.h>

// From the wiki.
static const char test_log1[] = \
  "0000815169,15296,0036,00550,0,00000,0000,00000,0,00,00000,0000,00000,0,00,14,12\r\n";
static_assert(sizeof(test_log1) == OSP3_LOG_PROTOCOL_SIZE + 1, "incorrect log buffer length");

// From the device.
static const char test_log2[] = \
  "0343732187,15321,0072,01103,0,00000,0000,00000,0,00,00000,0000,00000,0,00,1c,12\r\n";
static_assert(sizeof(test_log2) == OSP3_LOG_PROTOCOL_SIZE + 1, "incorrect log buffer length");
static const char test_log3[] = \
  "0343732197,15332,0084,01287,0,00000,0000,00000,0,00,00000,0000,00000,0,00,09,17\r\n";
static_assert(sizeof(test_log3) == OSP3_LOG_PROTOCOL_SIZE + 1, "incorrect log buffer length");
static const char test_log4[] = \
  "0343732207,15328,0055,00843,0,00000,0000,00000,0,00,00000,0000,00000,0,00,11,19\r\n";
static_assert(sizeof(test_log4) == OSP3_LOG_PROTOCOL_SIZE + 1, "incorrect log buffer length");

// Doesn't include the trailing "\r\n".
static const char test_log_no_newline[] = \
  "0000815169,15296,0036,00550,0,00000,0000,00000,0,00,00000,0000,00000,0,00,14,12";
static_assert(sizeof(test_log_no_newline) == OSP3_LOG_PROTOCOL_SIZE - 1, "incorrect log buffer length");

static void test_osp3_open_path_bad(void) {
  errno = 0;
  assert(osp3_open_path(NULL, 0) == NULL);
  assert(errno == EINVAL);
  // TODO: To test bad baud values without trying to open a device, osp3_open_path would have to be refactored.
}

static void test_osp3_close_bad(void) {
  errno = 0;
  assert(osp3_close(NULL) == -1);
  assert(errno == EINVAL);
}

static void test_osp3_flush_bad(void) {
  errno = 0;
  assert(osp3_flush(NULL) == -1);
  assert(errno == EINVAL);
}

static void test_osp3_read_bad(void) {
  int dummy = 0;
  size_t transferred;
  unsigned char buf[OSP3_LOG_PROTOCOL_SIZE];
  // NULL arguments.
  errno = 0;
  assert(osp3_read(NULL, buf, sizeof(buf), &transferred, 0) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_read((osp3_device*) &dummy, NULL, sizeof(buf), &transferred, 0) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_read((osp3_device*) &dummy, buf, sizeof(buf), NULL, 0) == -1);
  assert(errno == EINVAL);
}

static void test_osp3_read_line_bad(void) {
  int dummy = 0;
  size_t transferred;
  unsigned char buf[OSP3_LOG_PROTOCOL_SIZE];
  // NULL arguments.
  errno = 0;
  assert(osp3_read_line(NULL, buf, sizeof(buf), &transferred, 0) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_read_line((osp3_device*) &dummy, NULL, sizeof(buf), &transferred, 0) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_read_line((osp3_device*) &dummy, buf, sizeof(buf), NULL, 0) == -1);
  assert(errno == EINVAL);
}

static void test_osp3_log_checksum_bad(void) {
  uint8_t cs8_2s = 0;
  uint8_t cs8_xor = 0;
  // NULL arguments.
  errno = 0;
  assert(osp3_log_checksum(NULL, sizeof(test_log1), &cs8_2s, &cs8_xor) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_log_checksum(test_log1, sizeof(test_log1), NULL, &cs8_xor) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_log_checksum(test_log1, sizeof(test_log1), &cs8_2s, NULL) == -1);
  assert(errno == EINVAL);
  // Bad size.
  errno = 0;
  assert(osp3_log_checksum(test_log1, OSP3_LOG_PROTOCOL_SIZE - 2, &cs8_2s, &cs8_xor) == -1);
  assert(errno == EINVAL);
}

static void test_osp3_log_checksum(void) {
  uint8_t cs8_2s = 0;
  uint8_t cs8_xor = 0;
  assert(osp3_log_checksum(test_log1, sizeof(test_log1), &cs8_2s, &cs8_xor) == 0);
  assert(cs8_2s == 0x14);
  assert(cs8_xor == 0x12);
  assert(osp3_log_checksum(test_log2, sizeof(test_log2), &cs8_2s, &cs8_xor) == 0);
  assert(cs8_2s == 0x1c);
  assert(cs8_xor == 0x12);
  assert(osp3_log_checksum(test_log3, sizeof(test_log3), &cs8_2s, &cs8_xor) == 0);
  assert(cs8_2s == 0x09);
  assert(cs8_xor == 0x17);
  assert(osp3_log_checksum(test_log4, sizeof(test_log4), &cs8_2s, &cs8_xor) == 0);
  assert(cs8_2s == 0x11);
  assert(cs8_xor == 0x19);
  // Bad checksums (modified test_log1).
  static const char test_log1_bad_2s[] = \
    "0000815169,15296,0036,00550,0,00000,0000,00000,0,00,00000,0000,00000,0,00,15,12\r\n";
  static const char test_log1_bad_xor[] = \
    "0000815169,15296,0036,00550,0,00000,0000,00000,0,00,00000,0000,00000,0,00,14,13\r\n";
  assert(osp3_log_checksum(test_log1_bad_2s, sizeof(test_log1_bad_2s), &cs8_2s, &cs8_xor) == 1);
  assert(osp3_log_checksum(test_log1_bad_xor, sizeof(test_log1_bad_xor), &cs8_2s, &cs8_xor) == 1);
  // No newline characters.
  assert(osp3_log_checksum(test_log_no_newline, sizeof(test_log_no_newline), &cs8_2s, &cs8_xor) == 0);
}

static void test_osp3_log_checksum_test_bad(void) {
  // NULL arguments.
  errno = 0;
  assert(osp3_log_checksum_test(NULL, sizeof(test_log1), 0x14, 0x12) == -1);
  assert(errno == EINVAL);
  // Bad size.
  errno = 0;
  assert(osp3_log_checksum_test(test_log1, OSP3_LOG_PROTOCOL_SIZE - 2, 0x14, 0x12) == -1);
  assert(errno == EINVAL);
}

static void test_osp3_log_checksum_test(void) {
  assert(osp3_log_checksum_test(test_log1, sizeof(test_log1), 0x14, 0x12) == 0);
  assert(osp3_log_checksum_test(test_log2, sizeof(test_log2), 0x1c, 0x12) == 0);
  assert(osp3_log_checksum_test(test_log3, sizeof(test_log3), 0x09, 0x17) == 0);
  assert(osp3_log_checksum_test(test_log4, sizeof(test_log4), 0x11, 0x19) == 0);
  // Bad checksums.
  assert(osp3_log_checksum_test(test_log1, sizeof(test_log1), 0x15, 0x12) == 1);
  assert(osp3_log_checksum_test(test_log1, sizeof(test_log1), 0x14, 0x13) == 1);
  // No newline characters.
  assert(osp3_log_checksum_test(test_log_no_newline, sizeof(test_log_no_newline), 0x14, 0x12) == 0);
}

static void test_osp3_log_parse_bad(void) {
  osp3_log_entry log_entry;
  // NULL arguments.
  errno = 0;
  assert(osp3_log_parse(NULL, sizeof(test_log1), &log_entry) == -1);
  assert(errno == EINVAL);
  errno = 0;
  assert(osp3_log_parse(test_log1, sizeof(test_log1), NULL) == -1);
  assert(errno == EINVAL);
  // Bad size.
  assert(osp3_log_parse(test_log1, OSP3_LOG_PROTOCOL_SIZE - 2, &log_entry) == -1);
}

static void test_osp3_log_parse(void) {
  osp3_log_entry log_entry;
  // Something that's not 0.
  memset(&log_entry, 0xFF, sizeof(log_entry));
  assert(osp3_log_parse(test_log1, sizeof(test_log1), &log_entry) == 0);
  assert(log_entry.ms == 815169);
  assert(log_entry.mV_in == 15296);
  assert(log_entry.mA_in == 36);
  assert(log_entry.mW_in == 550);
  assert(log_entry.onoff_in == 0);
  assert(log_entry.mV_0 == 0);
  assert(log_entry.mA_0 == 0);
  assert(log_entry.mW_0 == 0);
  assert(log_entry.onoff_0 == 0);
  assert(log_entry.intr_0 == 0);
  assert(log_entry.mV_1 == 0);
  assert(log_entry.mA_1 == 0);
  assert(log_entry.mW_1 == 0);
  assert(log_entry.onoff_1 == 0);
  assert(log_entry.intr_0 == 0);
  assert(log_entry.checksum8_2s_compl == 0x14);
  assert(log_entry.checksum8_xor == 0x12);
  // test_log2 checksum values include letters (hexadecimal).
  assert(osp3_log_parse(test_log2, sizeof(test_log2), &log_entry) == 0);
  assert(log_entry.checksum8_2s_compl == 0x1c);
  assert(log_entry.checksum8_xor == 0x12);
  // No newline characters.
  assert(osp3_log_parse(test_log_no_newline, sizeof(test_log_no_newline), &log_entry) == 0);
}

int main(void) {
  test_osp3_open_path_bad();
  test_osp3_close_bad();
  test_osp3_flush_bad();
  test_osp3_read_bad();
  test_osp3_read_line_bad();
  test_osp3_log_checksum_bad();
  test_osp3_log_checksum();
  test_osp3_log_checksum_test_bad();
  test_osp3_log_checksum_test();
  test_osp3_log_parse_bad();
  test_osp3_log_parse();
  return 0;
}
