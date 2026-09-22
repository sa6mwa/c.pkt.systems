#include <cpkt/sqlite.h>

static int compare(void *context, int left_byte_count, const void *left,
                   int right_byte_count, const void *right) {
  (void)context;
  (void)left_byte_count;
  (void)left;
  (void)right_byte_count;
  (void)right;
  return 0;
}

static void destroy(void *context) {
  int *destroy_count;
  destroy_count = (int *)context;
  if (destroy_count != 0)
    *destroy_count += 1;
}

int main(void) {
  cpkt_sqlite *database;
  int destroy_count;
  int status;

  destroy_count = 0;
  database = cpkt_sqlite_new(":memory:");
  if (database == 0)
    return 1;
  status = cpkt_sqlite_create_collation(database, "invalid-collation", 0,
                                        &destroy_count, compare, destroy);
  if (status != CPKT_SQLITE_MISUSE || destroy_count != 0) {
    database->close(database);
    return 2;
  }
  database->close(database);
  return 0;
}
