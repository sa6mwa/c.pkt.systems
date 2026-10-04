# Staging is separate from explicit test commands and requires their actual proof.
if(CPKT_GROUP STREQUAL "all")
  set(_cpkt_package_action stage-target)
else()
  set(_cpkt_package_action stage)
endif()
cpkt_group_add_custom_target(package-bundle
  COMMAND "${CPKT_OPERATION_PYTHON}" "${CMAKE_SOURCE_DIR}/scripts/cpkt_packages.py"
    "${_cpkt_package_action}" --group "${CPKT_GROUP}" --preset "$ENV{CPKT_PRESET}"
  VERBATIM)
