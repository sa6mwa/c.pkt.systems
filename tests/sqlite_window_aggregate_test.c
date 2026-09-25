#include <cpkt/sqlite.h>

static void aggregate_step(cpkt_sqlite_context *context, int argument_count,
                           cpkt_sqlite_value *const *arguments,
                           void *user_data) {
  (void)context;
  (void)argument_count;
  (void)arguments;
  (void)user_data;
}

static void aggregate_final(cpkt_sqlite_context *context, int argument_count,
                            cpkt_sqlite_value *const *arguments,
                            void *user_data) {
  (void)argument_count;
  (void)arguments;
  (void)user_data;
  cpkt_sqlite_context_result_int(context,
                                 cpkt_sqlite_context_aggregate_count(context));
}

static void destroy_binding(void *context) { ++*(int *)context; }

int main(void) {
  cpkt_sqlite *database;
  cpkt_sqlite_statement *statement;
  int destroyed;
  int status;
  destroyed = 0;
  statement = NULL;
  database = cpkt_sqlite_new(":memory:");
  if (database == NULL || database->error_code(database) != CPKT_SQLITE_OK)
    return 1;
  status = cpkt_sqlite_create_window_function(
      database, "cpkt_aggregate", 1, CPKT_SQLITE_UTF8, &destroyed,
      aggregate_step, aggregate_final, NULL, NULL, destroy_binding);
  if (status != CPKT_SQLITE_OK)
    return 2;
  status = cpkt_sqlite_create_window_function(
      database, "invalid_window", 1, CPKT_SQLITE_UTF8, NULL, aggregate_step,
      aggregate_final, aggregate_final, NULL, NULL);
  if (status != CPKT_SQLITE_MISUSE)
    return 3;
  status =
      database->prepare(database,
                        "select cpkt_aggregate(v) from "
                        "(select 1 as v union all select 2 union all select 3)",
                        -1, 0, &statement, NULL);
  if (status != CPKT_SQLITE_OK || statement == NULL)
    return 4;
  if (statement->step(statement) != CPKT_SQLITE_ROW ||
      statement->column_int(statement, 0) != 3 ||
      statement->step(statement) != CPKT_SQLITE_DONE ||
      statement->finalize(statement) != CPKT_SQLITE_OK)
    return 5;
  database->close(database);
  return destroyed == 1 ? 0 : 6;
}
