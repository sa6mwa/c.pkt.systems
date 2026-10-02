#include <cpkt/sqlite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct probe_counts {
  int create_calls;
  int connect_calls;
  int table_cleanup;
  int open_calls;
  int cursor_cleanup;
  int table_live;
  int cursor_live;
  int fail_table_adapter;
  int fail_cursor_adapter;
  int refuse_connect;
  int refuse_open;
  int callback_failure_cleanup;
} probe_counts;

static int fail_next_calloc;
void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size) {
  if (fail_next_calloc) {
    fail_next_calloc = 0;
    return NULL;
  }
  return __real_calloc(count, size);
}

static int table_connect(void *context, cpkt_sqlite *database,
                         int argument_count, const char *const *arguments,
                         cpkt_sqlite_virtual_table **out, char **error_out,
                         int create) {
  probe_counts *counts;
  cpkt_sqlite_virtual_table *table;
  void *state;
  (void)argument_count;
  (void)arguments;
  counts = (probe_counts *)context;
  if (create)
    ++counts->create_calls;
  else
    ++counts->connect_calls;
  *out = NULL;
  if (counts->refuse_connect) {
    state = malloc(1);
    if (state == NULL)
      return CPKT_SQLITE_NOMEM;
    table = cpkt_sqlite_virtual_table_new(state);
    if (table == NULL) {
      free(state);
      return CPKT_SQLITE_NOMEM;
    }
    free(table->state);
    free(table);
    ++counts->callback_failure_cleanup;
    *error_out = cpkt_sqlite_format("callback refusal");
    return CPKT_SQLITE_ERROR;
  }
  if (cpkt_sqlite_declare_virtual_table(
          database, "CREATE TABLE x(value INTEGER)") != CPKT_SQLITE_OK)
    return CPKT_SQLITE_ERROR;
  state = malloc(1);
  if (state == NULL)
    return CPKT_SQLITE_NOMEM;
  table = cpkt_sqlite_virtual_table_new(state);
  if (table == NULL) {
    free(state);
    return CPKT_SQLITE_NOMEM;
  }
  ++counts->table_live;
  *out = table;
  if (counts->fail_table_adapter)
    fail_next_calloc = 1;
  return CPKT_SQLITE_OK;
}

static int on_create(void *context, cpkt_sqlite *database, int argument_count,
                     const char *const *arguments,
                     cpkt_sqlite_virtual_table **out, char **error_out) {
  return table_connect(context, database, argument_count, arguments, out,
                       error_out, 1);
}

static int on_connect(void *context, cpkt_sqlite *database, int argument_count,
                      const char *const *arguments,
                      cpkt_sqlite_virtual_table **out, char **error_out) {
  return table_connect(context, database, argument_count, arguments, out,
                       error_out, 0);
}

static int on_table_cleanup(void *context, cpkt_sqlite_virtual_table *table) {
  probe_counts *counts;
  counts = (probe_counts *)context;
  ++counts->table_cleanup;
  --counts->table_live;
  free(table->state);
  table->state = NULL;
  return CPKT_SQLITE_OK;
}

static int on_best_index(void *context, cpkt_sqlite_virtual_table *table,
                         cpkt_sqlite_index_info *info) {
  (void)context;
  (void)table;
  info->estimated_cost = 1.0;
  return CPKT_SQLITE_OK;
}

static int on_open(void *context, cpkt_sqlite_virtual_table *table,
                   cpkt_sqlite_virtual_cursor **out) {
  probe_counts *counts;
  cpkt_sqlite_virtual_cursor *cursor;
  void *state;
  counts = (probe_counts *)context;
  ++counts->open_calls;
  *out = NULL;
  if (counts->refuse_open) {
    state = malloc(1);
    if (state == NULL)
      return CPKT_SQLITE_NOMEM;
    cursor = cpkt_sqlite_virtual_cursor_new(table, state);
    if (cursor == NULL) {
      free(state);
      return CPKT_SQLITE_NOMEM;
    }
    free(cursor->state);
    free(cursor);
    ++counts->callback_failure_cleanup;
    return CPKT_SQLITE_ERROR;
  }
  state = malloc(1);
  if (state == NULL)
    return CPKT_SQLITE_NOMEM;
  cursor = cpkt_sqlite_virtual_cursor_new(table, state);
  if (cursor == NULL) {
    free(state);
    return CPKT_SQLITE_NOMEM;
  }
  ++counts->cursor_live;
  *out = cursor;
  if (counts->fail_cursor_adapter)
    fail_next_calloc = 1;
  return CPKT_SQLITE_OK;
}

static int on_close(void *context, cpkt_sqlite_virtual_cursor *cursor) {
  probe_counts *counts;
  counts = (probe_counts *)context;
  ++counts->cursor_cleanup;
  --counts->cursor_live;
  free(cursor->state);
  cursor->state = NULL;
  return CPKT_SQLITE_OK;
}

static int on_filter(void *context, cpkt_sqlite_virtual_cursor *cursor,
                     int index_number, const char *index_string,
                     int argument_count, cpkt_sqlite_value *const *arguments) {
  (void)context;
  (void)cursor;
  (void)index_number;
  (void)index_string;
  (void)argument_count;
  (void)arguments;
  return CPKT_SQLITE_OK;
}

static int on_next(void *context, cpkt_sqlite_virtual_cursor *cursor) {
  (void)context;
  (void)cursor;
  return CPKT_SQLITE_OK;
}

static int on_eof(void *context, cpkt_sqlite_virtual_cursor *cursor) {
  (void)context;
  (void)cursor;
  return 1;
}

static int on_column(void *context, cpkt_sqlite_virtual_cursor *cursor,
                     cpkt_sqlite_context *result, int column) {
  (void)context;
  (void)cursor;
  (void)result;
  (void)column;
  return CPKT_SQLITE_OK;
}

static int on_rowid(void *context, cpkt_sqlite_virtual_cursor *cursor,
                    cpkt_sqlite_i64 *rowid) {
  (void)context;
  (void)cursor;
  *rowid = cpkt_sqlite_i64_make(0UL, 1UL);
  return CPKT_SQLITE_OK;
}

static int run_case(int kind) {
  probe_counts counts;
  cpkt_sqlite_module_methods methods;
  cpkt_sqlite *database;
  const char *sql;
  int status;
  int failed;
  memset(&counts, 0, sizeof(counts));
  memset(&methods, 0, sizeof(methods));
  counts.fail_table_adapter = kind == 1 || kind == 2;
  counts.fail_cursor_adapter = kind == 3;
  counts.refuse_connect = kind == 4;
  counts.refuse_open = kind == 5;
  methods.context = &counts;
  /* Equal native xCreate/xConnect pointers enable eponymous lookup. */
  methods.create = kind == 1 ? on_create : on_connect;
  methods.connect = on_connect;
  methods.best_index = on_best_index;
  methods.disconnect = on_table_cleanup;
  methods.destroy_table = on_table_cleanup;
  methods.open = on_open;
  methods.close = on_close;
  methods.filter = on_filter;
  methods.next = on_next;
  methods.eof = on_eof;
  methods.column = on_column;
  methods.rowid = on_rowid;
  database = cpkt_sqlite_new(":memory:");
  if (database == NULL ||
      cpkt_sqlite_create_module(database, "probe", &methods) != CPKT_SQLITE_OK)
    return 0;
  sql =
      kind == 1 ? "CREATE VIRTUAL TABLE t USING probe" : "SELECT * FROM probe";
  status = database->tx(database, sql, NULL, NULL);
  fail_next_calloc = 0;
  /* Once the adapter moves before the callback, a one-shot libc fault may
   * instead strike a later SQLite allocation. Either status is valid then;
   * the successfully created table must still receive one cleanup. */
  failed =
      status != (kind == 4 || kind == 5 ? CPKT_SQLITE_ERROR : CPKT_SQLITE_OK) &&
      !(kind == 2 && status == CPKT_SQLITE_NOMEM);
  if (kind == 4 &&
      (database->error(database) == NULL ||
       strstr(database->error(database), "callback refusal") == NULL))
    failed = 1;
  if (kind == 1 && status == CPKT_SQLITE_OK &&
      database->tx(database, "DROP TABLE t", NULL, NULL) != CPKT_SQLITE_OK)
    failed = 1;
  database->close(database);
  if (counts.table_live != 0 || counts.cursor_live != 0 ||
      counts.callback_failure_cleanup != (kind == 4 || kind == 5 ? 1 : 0) ||
      counts.table_cleanup !=
          counts.create_calls + counts.connect_calls - (kind == 4 ? 1 : 0) ||
      counts.cursor_cleanup != counts.open_calls - (kind == 5 ? 1 : 0))
    failed = 1;
  if (failed)
    fprintf(stderr,
            "module case %d: status=%d create=%d connect=%d table_cleanup=%d "
            "open=%d cursor_cleanup=%d live=%d/%d\n",
            kind, status, counts.create_calls, counts.connect_calls,
            counts.table_cleanup, counts.open_calls, counts.cursor_cleanup,
            counts.table_live, counts.cursor_live);
  return !failed;
}

int main(void) {
  int kind;
  int failed;
  failed = 0;
  for (kind = 1; kind <= 6; ++kind)
    if (!run_case(kind))
      failed = 1;
  return failed;
}
