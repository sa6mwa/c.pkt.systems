cmake_minimum_required(VERSION 3.21)
if(NOT CPKT_SOURCE_DIR OR NOT CPKT_GROUP OR NOT CPKT_PRESET)
  message(FATAL_ERROR "package_bundle requires SOURCE_DIR, GROUP and explicit Release PRESET; use make package")
endif()
find_program(_python NAMES python3 REQUIRED)
execute_process(COMMAND "${_python}" "${CPKT_SOURCE_DIR}/scripts/cpkt_packages.py"
  stage --group "${CPKT_GROUP}" --preset "${CPKT_PRESET}"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "group packaging failed")
endif()
