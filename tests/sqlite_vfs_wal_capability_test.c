#include <cpkt/sqlite.h>

#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3_vfs *base_vfs;
static cpkt_sqlite_io_methods io_methods;

static sqlite3_file *native_file(cpkt_sqlite_file *file) {
  return (sqlite3_file *)file->state;
}

static sqlite3_int64 native_offset(cpkt_sqlite_i64 offset) {
  return ((sqlite3_int64)(int32_t)offset.high << 32) |
         (sqlite3_int64)(uint32_t)offset.low;
}

static int file_close(cpkt_sqlite_file *file) {
  sqlite3_file *native = native_file(file);
  int status = native->pMethods->xClose(native);
  free(native);
  return status;
}

static int file_read(cpkt_sqlite_file *file, void *buffer, int count,
                     cpkt_sqlite_i64 offset) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xRead(native, buffer, count, native_offset(offset));
}

static int file_write(cpkt_sqlite_file *file, const void *buffer, int count,
                      cpkt_sqlite_i64 offset) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xWrite(native, buffer, count, native_offset(offset));
}

static int file_truncate(cpkt_sqlite_file *file, cpkt_sqlite_i64 size) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xTruncate(native, native_offset(size));
}

static int file_sync(cpkt_sqlite_file *file, int flags) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xSync(native, flags);
}

static int file_size(cpkt_sqlite_file *file, cpkt_sqlite_i64 *size_out) {
  sqlite3_file *native = native_file(file);
  sqlite3_int64 size;
  int status = native->pMethods->xFileSize(native, &size);
  if (status == SQLITE_OK) {
    size_out->high = (unsigned long)((uint64_t)size >> 32);
    size_out->low = (unsigned long)((uint64_t)size & UINT32_MAX);
  }
  return status;
}

static int file_lock(cpkt_sqlite_file *file, int level) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xLock(native, level);
}

static int file_unlock(cpkt_sqlite_file *file, int level) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xUnlock(native, level);
}

static int file_reserved_lock(cpkt_sqlite_file *file, int *result_out) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xCheckReservedLock(native, result_out);
}

static int file_control(cpkt_sqlite_file *file, int operation, void *argument) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xFileControl(native, operation, argument);
}

static int file_sector_size(cpkt_sqlite_file *file) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xSectorSize(native);
}

static int file_characteristics(cpkt_sqlite_file *file) {
  sqlite3_file *native = native_file(file);
  return native->pMethods->xDeviceCharacteristics(native);
}

static int vfs_open(cpkt_sqlite_vfs *vfs, const char *name,
                    cpkt_sqlite_file *file, int flags, int *flags_out) {
  sqlite3_file *native;
  int status;
  (void)vfs;
  native = (sqlite3_file *)calloc(1, (size_t)base_vfs->szOsFile);
  if (native == NULL)
    return SQLITE_NOMEM;
  status = base_vfs->xOpen(base_vfs, name, native, flags, flags_out);
  if (status != SQLITE_OK) {
    free(native);
    return status;
  }
  file->state = native;
  file->methods = &io_methods;
  return SQLITE_OK;
}

static int vfs_delete(cpkt_sqlite_vfs *vfs, const char *name, int sync_dir) {
  (void)vfs;
  return base_vfs->xDelete(base_vfs, name, sync_dir);
}

static int vfs_access(cpkt_sqlite_vfs *vfs, const char *name, int flags,
                      int *result_out) {
  (void)vfs;
  return base_vfs->xAccess(base_vfs, name, flags, result_out);
}

static int vfs_full_path(cpkt_sqlite_vfs *vfs, const char *name, int size,
                         char *output) {
  (void)vfs;
  return base_vfs->xFullPathname(base_vfs, name, size, output);
}

static int vfs_randomness(cpkt_sqlite_vfs *vfs, int size, char *output) {
  (void)vfs;
  return base_vfs->xRandomness(base_vfs, size, output);
}

static int vfs_sleep(cpkt_sqlite_vfs *vfs, int microseconds) {
  (void)vfs;
  return base_vfs->xSleep(base_vfs, microseconds);
}

static int vfs_current_time(cpkt_sqlite_vfs *vfs, double *time_out) {
  (void)vfs;
  return base_vfs->xCurrentTime(base_vfs, time_out);
}

static int capture_journal_mode(void *context, int count,
                                const char *const *values,
                                const char *const *names) {
  char *mode = (char *)context;
  (void)names;
  if (count > 0 && values[0] != NULL)
    snprintf(mode, 16, "%s", values[0]);
  return 0;
}

int main(int argc, char **argv) {
  cpkt_sqlite_vfs_methods methods;
  cpkt_sqlite_vfs *vfs;
  cpkt_sqlite *database;
  char journal_mode[16] = {0};
  int status;

  if (argc != 2 || cpkt_sqlite_initialize() != CPKT_SQLITE_OK)
    return 1;
  base_vfs = sqlite3_vfs_find(NULL);
  if (base_vfs == NULL)
    return 2;
  memset(&io_methods, 0, sizeof(io_methods));
  io_methods.version = 3;
  io_methods.close = file_close;
  io_methods.read = file_read;
  io_methods.write = file_write;
  io_methods.truncate = file_truncate;
  io_methods.sync = file_sync;
  io_methods.size = file_size;
  io_methods.lock = file_lock;
  io_methods.unlock = file_unlock;
  io_methods.check_reserved_lock = file_reserved_lock;
  io_methods.control = file_control;
  io_methods.sector_size = file_sector_size;
  io_methods.characteristics = file_characteristics;
  memset(&methods, 0, sizeof(methods));
  methods.version = 1;
  methods.open = vfs_open;
  methods.delete_file = vfs_delete;
  methods.access = vfs_access;
  methods.full_path = vfs_full_path;
  methods.randomness = vfs_randomness;
  methods.sleep = vfs_sleep;
  methods.current_time = vfs_current_time;
  vfs = cpkt_sqlite_vfs_new("cpkt-no-shm-vfs", 1024, NULL, &methods);
  if (vfs == NULL || vfs->register_vfs(vfs, 0) != CPKT_SQLITE_OK)
    return 3;
  (void)remove(argv[1]);
  database = cpkt_sqlite_open(
      argv[1], CPKT_SQLITE_OPEN_READWRITE | CPKT_SQLITE_OPEN_CREATE,
      "cpkt-no-shm-vfs");
  if (database == NULL)
    return 4;
  status = database->tx(database, "PRAGMA journal_mode=WAL",
                        capture_journal_mode, journal_mode);
  if (status != CPKT_SQLITE_OK || strcmp(journal_mode, "delete") != 0)
    return 5;
  status = database->tx(database, "CREATE TABLE t(value INTEGER)", NULL, NULL);
  database->close(database);
  vfs->close(vfs);
  (void)remove(argv[1]);
  return status == CPKT_SQLITE_OK ? 0 : 6;
}
