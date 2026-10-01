#include <cpkt/sqlite.h>

#include <stdio.h>
#include <string.h>

int cpkt_test_borrowed_file_view(cpkt_sqlite *database, int *close_calls);
int cpkt_test_vfs_provider_identity(cpkt_sqlite *database,
                                    cpkt_sqlite_vfs *original);

int cpkt_test_borrowed_file_view(cpkt_sqlite *database, int *close_calls) {
  cpkt_sqlite_file_handle *first;
  cpkt_sqlite_file_handle *second;
  cpkt_sqlite_file *view;
  cpkt_sqlite_i64 size;
  int before;
  first = NULL;
  second = NULL;
  if (database->tx(database, "CREATE TABLE borrowed(value INTEGER)", NULL,
                   NULL) != CPKT_SQLITE_OK ||
      cpkt_sqlite_file_control_file(database, "main",
                                    CPKT_SQLITE_FCNTL_FILE_POINTER,
                                    &first) != CPKT_SQLITE_OK ||
      cpkt_sqlite_file_control_file(database, "main",
                                    CPKT_SQLITE_FCNTL_FILE_POINTER,
                                    &second) != CPKT_SQLITE_OK)
    return 30;
  view = cpkt_sqlite_file_handle_view(first);
  before = *close_calls;
  if (view == NULL || view->methods->close(view) != CPKT_SQLITE_MISUSE ||
      *close_calls != before || view == cpkt_sqlite_file_handle_view(second)) {
    fprintf(stderr,
            "borrowed file view exposed an owning close or shared metadata\n");
    return 31;
  }
  if (view->methods->size(view, &size) != CPKT_SQLITE_OK ||
      cpkt_sqlite_file_handle_size(first, &size) != CPKT_SQLITE_OK)
    return 32;
  cpkt_sqlite_file_handle_close(first);
  view = cpkt_sqlite_file_handle_view(second);
  if (*close_calls != before ||
      view->methods->close(view) != CPKT_SQLITE_MISUSE ||
      view->methods->size(view, &size) != CPKT_SQLITE_OK)
    return 33;
  cpkt_sqlite_file_handle_close(second);
  if (*close_calls != before ||
      database->tx(database, "INSERT INTO borrowed VALUES(1)", NULL, NULL) !=
          CPKT_SQLITE_OK)
    return 34;
  if (database->tx(database, "BEGIN; INSERT INTO borrowed VALUES(2)", NULL,
                   NULL) != CPKT_SQLITE_OK ||
      cpkt_sqlite_file_control_file(database, "main",
                                    CPKT_SQLITE_FCNTL_JOURNAL_POINTER,
                                    &first) != CPKT_SQLITE_OK)
    return 35;
  before = *close_calls;
  view = cpkt_sqlite_file_handle_view(first);
  if (view == NULL || view->methods->close(view) != CPKT_SQLITE_MISUSE ||
      *close_calls != before ||
      view->methods->size(view, &size) != CPKT_SQLITE_OK)
    return 36;
  cpkt_sqlite_file_handle_close(first);
  if (*close_calls != before ||
      database->tx(database, "COMMIT", NULL, NULL) != CPKT_SQLITE_OK)
    return 37;
  return 0;
}

static int check_provider(cpkt_sqlite_vfs_handle *handle) {
  cpkt_sqlite_vfs *view;
  int direct;
  int through_view;
  direct = 0;
  through_view = 0;
  view = cpkt_sqlite_vfs_handle_view(handle);
  return view == NULL ||
         cpkt_sqlite_vfs_handle_access(handle, "cpkt-vfs-identity-probe", 0,
                                       &direct) != CPKT_SQLITE_OK ||
         view->methods.access(view, "cpkt-vfs-identity-probe", 0,
                              &through_view) != CPKT_SQLITE_OK ||
         direct != 11 || through_view != 11;
}

int cpkt_test_vfs_provider_identity(cpkt_sqlite *database,
                                    cpkt_sqlite_vfs *original) {
  cpkt_sqlite_vfs *replacement;
  cpkt_sqlite_vfs *found;
  cpkt_sqlite_vfs_handle *before;
  cpkt_sqlite_vfs_handle *after;
  cpkt_sqlite_vfs_handle *unregistered;
  int marker;
  marker = 22;
  before = NULL;
  after = NULL;
  unregistered = NULL;
  if (cpkt_sqlite_file_control_vfs(database, "main", &before) != CPKT_SQLITE_OK)
    return 40;
  replacement =
      cpkt_sqlite_vfs_new(original->name, original->maximum_pathname_bytes,
                          &marker, &original->methods);
  if (replacement == NULL ||
      replacement->register_vfs(replacement, 0) != CPKT_SQLITE_OK)
    return 41;
  found = cpkt_sqlite_vfs_find(original->name);
  if (found != replacement ||
      cpkt_sqlite_file_control_vfs(database, "main", &after) !=
          CPKT_SQLITE_OK ||
      check_provider(before) || check_provider(after)) {
    fprintf(stderr,
            "database handle view resolved a replacement provider by name\n");
    return 42;
  }
  if (original->unregister_vfs(original) != CPKT_SQLITE_OK ||
      replacement->unregister_vfs(replacement) != CPKT_SQLITE_OK ||
      cpkt_sqlite_vfs_find(original->name) != NULL ||
      cpkt_sqlite_file_control_vfs(database, "main", &unregistered) !=
          CPKT_SQLITE_OK ||
      check_provider(unregistered)) {
    fprintf(stderr, "database handle lost its unregistered provider\n");
    return 43;
  }
  cpkt_sqlite_vfs_handle_close(before);
  cpkt_sqlite_vfs_handle_close(after);
  if (check_provider(unregistered))
    return 44;
  cpkt_sqlite_vfs_handle_close(unregistered);
  replacement->close(replacement);
  return 0;
}
