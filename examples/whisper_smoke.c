#include <stddef.h>

#include <whisper.h>

int main(void) {
  const char *version;
  const char *system_info;

  version = whisper_version();
  if (version == NULL || version[0] == '\0') {
    return 1;
  }

  system_info = whisper_print_system_info();
  if (system_info == NULL || system_info[0] == '\0') {
    return 2;
  }

  return 0;
}
