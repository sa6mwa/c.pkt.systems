#include <string.h>

#include <cpkt/sus.h>

int cpkt_sus_mixed_cxx_value(void);

int main(void) {
  const char *capabilities;

  capabilities = cpkt_sus_backend_capabilities();
  if (capabilities == 0 || strcmp(capabilities, "cpu") != 0) {
    return 1;
  }
  return cpkt_sus_mixed_cxx_value() == 8 ? 0 : 2;
}
