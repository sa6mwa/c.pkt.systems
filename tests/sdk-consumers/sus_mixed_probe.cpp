#include <string>

extern "C" int cpkt_sus_mixed_cxx_value(void) {
  std::string value("cpkt-sus");
  return static_cast<int>(value.size());
}
