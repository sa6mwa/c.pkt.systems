if(NOT DEFINED CPKT_OPENLDAP_BUILD_DIR)
  message(FATAL_ERROR "CPKT_OPENLDAP_BUILD_DIR is required")
endif()
if(NOT DEFINED CPKT_OPENLDAP_MAKE_PROGRAM)
  message(FATAL_ERROR "CPKT_OPENLDAP_MAKE_PROGRAM is required")
endif()
if(NOT DEFINED CPKT_OPENLDAP_BUILD_JOBS)
  message(FATAL_ERROR "CPKT_OPENLDAP_BUILD_JOBS is required")
endif()

macro(cpkt_openldap_check directory)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "OpenLDAP build failed in ${directory} (exit ${result})")
  endif()
endmacro()

# Use OpenLDAP's root dispatcher.  It establishes the generated-header state
# required by its library graph.  OpenLDAP relies on make's built-in C object
# rules until an optional `make depend` creates explicit rules.  The parent
# c.pkt.systems Makefile exports --no-builtin-rules, so isolate the upstream
# build from that inherited setting rather than generating dependency files.
# Client utilities may be built transiently but are never staged or shipped by
# c.pkt.systems.
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MAKEFLAGS="
  "${CPKT_OPENLDAP_MAKE_PROGRAM}" all
  "-j${CPKT_OPENLDAP_BUILD_JOBS}"
  WORKING_DIRECTORY "${CPKT_OPENLDAP_BUILD_DIR}" RESULT_VARIABLE result)
cpkt_openldap_check(root libraries)
