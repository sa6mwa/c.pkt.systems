#include <cpkt/sqlite.h>

static int extension_calls;

static int initialize_extension(cpkt_sqlite *database, char **error_out,
                                void *context) {
  (void)error_out;
  (void)context;
  if (database == 0)
    return CPKT_SQLITE_MISUSE;
  ++extension_calls;
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
  if (extension->cancel(extension) != 1)
    return 6;
  extension->close(extension);
  return cpkt_sqlite_shutdown() == CPKT_SQLITE_OK ? 0 : 7;
}
