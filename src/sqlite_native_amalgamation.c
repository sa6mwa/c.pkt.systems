/* Keep the private log forwarding entry in the SQLite translation unit.
 * SQLite's renderer and logging configuration are intentionally private. */
#include "sqlite3.c"
#include <stdarg.h>

/* Keep the pinned amalgamation's existing diagnostic policy. Apply the same
 * warning groups as other project-owned C only after the upstream include. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wpedantic"
#endif

SQLITE_API void sqlite3__cpkt_log_v(int code, const char *format,
                                    va_list args) {
  if (sqlite3GlobalConfig.xLog != 0)
    renderLogMsg(code, format, args);
}

SQLITE_API void sqlite3__cpkt_log(int code, const char *format, ...) {
  va_list args;
  if (sqlite3GlobalConfig.xLog == 0)
    return;
  va_start(args, format);
  renderLogMsg(code, format, args);
  va_end(args);
}
