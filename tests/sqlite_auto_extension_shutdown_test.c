#include <cpkt/sqlite.h>

static int extension_calls;
static int fail_next_open;
static int stale_receiver_seen;

static int initialize_extension(cpkt_sqlite *database, char **error_out,
                                void *context) {
  (void)error_out;
  (void)context;
  if (database == 0)
    return CPKT_SQLITE_MISUSE;
  ++extension_calls;
  if (fail_next_open) {
    fail_next_open = 0;
    database->tx = 0;
    return CPKT_SQLITE_NOMEM;
  }
  if (database->tx == 0)
    stale_receiver_seen = 1;
  return CPKT_SQLITE_OK;
}

static int open_with_extension(int expected_calls) {
  cpkt_sqlite *database;
  database = cpkt_sqlite_open(
      ":memory:", CPKT_SQLITE_OPEN_READWRITE | CPKT_SQLITE_OPEN_CREATE, 0);
  if (database == 0)
    return 0;
  if (database->error_code(database) != CPKT_SQLITE_OK ||
      extension_calls != expected_calls) {
    database->close(database);
    return 0;
  }
  database->close(database);
  return 1;
}

int main(void) {
  cpkt_sqlite_auto_extension *extension;
  cpkt_sqlite *database;
  unsigned short utf16_filename[] = {':', 'm', 'e', 'm', 'o', 'r', 'y', ':', 0};
  if (cpkt_sqlite_initialize() != CPKT_SQLITE_OK)
    return 1;
  extension = cpkt_sqlite_auto_extension_new(initialize_extension, 0);
  if (extension == 0 ||
      extension->register_extension(extension) != CPKT_SQLITE_OK)
    return 2;
  if (!open_with_extension(1))
    return 3;
  if (cpkt_sqlite_shutdown() != CPKT_SQLITE_OK ||
      cpkt_sqlite_initialize() != CPKT_SQLITE_OK)
    return 4;
  if (extension->register_extension(extension) != CPKT_SQLITE_OK ||
      !open_with_extension(2))
    return 5;
  fail_next_open = 1;
  database = cpkt_sqlite_open(
      ":memory:", CPKT_SQLITE_OPEN_READWRITE | CPKT_SQLITE_OPEN_CREATE, 0);
  if (database != 0 || extension_calls != 3)
    return 8;
  if (!open_with_extension(4) || stale_receiver_seen)
    return 9;
  fail_next_open = 1;
  database = cpkt_sqlite_open16(utf16_filename);
  if (database != 0 || extension_calls != 5)
    return 10;
  database = cpkt_sqlite_open16(utf16_filename);
  if (database == 0 || database->error_code(database) != CPKT_SQLITE_OK ||
      extension_calls != 6 || stale_receiver_seen)
    return 11;
  database->close(database);
  if (extension->cancel(extension) != 1)
    return 6;
  extension->close(extension);
  return cpkt_sqlite_shutdown() == CPKT_SQLITE_OK ? 0 : 7;
}
