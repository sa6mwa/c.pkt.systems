#include <cpkt/sqlite.h>
#include <sqlite3.h>

#include <stdio.h>
#include <string.h>

static sqlite3_vfs *native_default;
static int journal_views;
static int journal_errors;

static int wrapped_open(sqlite3_vfs *vfs, const char *name, sqlite3_file *file,
                        int flags, int *flags_out) {
  cpkt_sqlite_file *view;
  cpkt_sqlite_i64 size;
  int index;
  (void)vfs;
  if ((flags & (SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_WAL)) != 0) {
    for (index = 0; index < 3; ++index) {
      view = cpkt_sqlite_vfs_database_file_object(name);
      if (view == NULL || view->methods == NULL ||
          view->methods->size == NULL || view->methods->close == NULL) {
        ++journal_errors;
        continue;
      }
      if (view->methods->size(view, &size) != SQLITE_OK)
        ++journal_errors;
      if (view->methods->close(view) != SQLITE_OK)
        ++journal_errors;
      ++journal_views;
    }
  }
  return native_default->xOpen(native_default, name, file, flags, flags_out);
}

static int direct_views(void) {
  cpkt_sqlite_vfs *view;
  cpkt_sqlite_vfs *custom;
  cpkt_sqlite_vfs_methods methods;
  int status;
  int errors;
  errors = 0;
  if (cpkt_sqlite_initialize() != SQLITE_OK)
    return 1;
  native_default = sqlite3_vfs_find(NULL);
  view = cpkt_sqlite_vfs_find(NULL);
  if (native_default == NULL || view == NULL || view->register_vfs == NULL ||
      view->unregister_vfs == NULL || view->close == NULL)
    return 2;
  if (cpkt_sqlite_vfs_register(NULL, 0) != SQLITE_MISUSE ||
      cpkt_sqlite_vfs_unregister(NULL) != SQLITE_MISUSE)
    ++errors;
  cpkt_sqlite_vfs_close(NULL);
  status = cpkt_sqlite_vfs_unregister(view);
  if (status != SQLITE_OK || sqlite3_vfs_find(native_default->zName) != NULL)
    ++errors;
  status = cpkt_sqlite_vfs_register(view, 1);
  if (status != SQLITE_OK || sqlite3_vfs_find(NULL) != native_default)
    ++errors;
  status = view->unregister_vfs(view);
  if (status != SQLITE_OK || sqlite3_vfs_find(native_default->zName) != NULL)
    ++errors;
  status = view->register_vfs(view, 1);
  if (status != SQLITE_OK || sqlite3_vfs_find(NULL) != native_default)
    ++errors;
  methods = view->methods;
  custom = cpkt_sqlite_vfs_new("cpkt-r3-custom-vfs", 1024, NULL, &methods);
  if (custom == NULL)
    ++errors;
  else {
    if (cpkt_sqlite_vfs_register(custom, 0) != SQLITE_OK ||
        cpkt_sqlite_vfs_find("cpkt-r3-custom-vfs") != custom ||
        cpkt_sqlite_vfs_unregister(custom) != SQLITE_OK ||
        sqlite3_vfs_find("cpkt-r3-custom-vfs") != NULL)
      ++errors;
    cpkt_sqlite_vfs_close(custom);
  }
  cpkt_sqlite_vfs_close(view);
  if (sqlite3_vfs_find(NULL) != native_default)
    ++errors;
  if (sqlite3_vfs_find(native_default->zName) != native_default &&
      sqlite3_vfs_register(native_default, 1) != SQLITE_OK)
    ++errors;
  if (cpkt_sqlite_shutdown() != SQLITE_OK)
    ++errors;
  if (errors)
    fprintf(stderr, "native VFS direct operations: %d errors\n", errors);
  return errors ? 3 : 0;
}

static int file_object(const char *path) {
  sqlite3_vfs wrapper;
  cpkt_sqlite *database;
  cpkt_sqlite_file_handle *handle;
  cpkt_sqlite_file *borrowed;
  cpkt_sqlite_i64 size;
  int errors;
  if (path == NULL)
    return 1;
  (void)remove(path);
  errors = 0;
  journal_views = 0;
  journal_errors = 0;
  if (cpkt_sqlite_initialize() != SQLITE_OK)
    return 2;
  native_default = sqlite3_vfs_find(NULL);
  if (native_default == NULL)
    return 3;
  wrapper = *native_default;
  wrapper.zName = "cpkt-r3-native-file-vfs";
  wrapper.xOpen = wrapped_open;
  if (sqlite3_vfs_register(&wrapper, 0) != SQLITE_OK)
    return 4;
  database = cpkt_sqlite_open(
      path, CPKT_SQLITE_OPEN_READWRITE | CPKT_SQLITE_OPEN_CREATE,
      wrapper.zName);
  handle = NULL;
  if (database == NULL)
    ++errors;
  else {
    if (database->tx(database,
                     "PRAGMA journal_mode=DELETE; CREATE TABLE t(x); "
                     "INSERT INTO t VALUES(1);",
                     NULL, NULL) != SQLITE_OK)
      ++errors;
    if (cpkt_sqlite_file_control_file(database, "main",
                                      CPKT_SQLITE_FCNTL_FILE_POINTER,
                                      &handle) != SQLITE_OK ||
        handle == NULL)
      ++errors;
    else {
      borrowed = cpkt_sqlite_file_handle_view(handle);
      if (borrowed == NULL || borrowed->methods == NULL ||
          borrowed->methods->close(borrowed) != SQLITE_MISUSE ||
          borrowed->methods->size(borrowed, &size) != SQLITE_OK)
        ++errors;
      cpkt_sqlite_file_handle_close(handle);
      handle = NULL;
    }
    if (database->tx(database, "INSERT INTO t VALUES(2);", NULL, NULL) !=
        SQLITE_OK)
      ++errors;
    database->close(database);
  }
  if (journal_views < 2 || journal_errors != 0)
    ++errors;
  if (sqlite3_vfs_unregister(&wrapper) != SQLITE_OK)
    ++errors;
  if (cpkt_sqlite_shutdown() != SQLITE_OK)
    ++errors;
  (void)remove(path);
  if (errors)
    fprintf(stderr, "native file metadata: views=%d errors=%d total=%d\n",
            journal_views, journal_errors, errors);
  return errors ? 5 : 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "direct") == 0)
    return direct_views();
  if (argc == 3 && strcmp(argv[1], "file") == 0)
    return file_object(argv[2]);
  return 2;
}
