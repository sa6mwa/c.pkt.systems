# Test-only libpslog must not enter either shipped facade library. Verify the
# new public entry points are real definitions in both library forms as well.
foreach(library IN ITEMS "${CPKT_STATIC}" "${CPKT_SHARED}")
  execute_process(COMMAND "${CPKT_NM}" -g "${library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not inspect OPC UA library: ${library}\n${error}")
  endif()
  if(symbols MATCHES "(^|\n)[^\n]*[ \t]_?pslog_[^\n]*")
    message(FATAL_ERROR "Shipped OPC UA library depends on libpslog: ${library}")
  endif()
  foreach(symbol IN ITEMS
      cpkt_opcua_server_new_with_logger
      cpkt_opcua_server_new_from_json_with_logger
      cpkt_opcua_server_new_from_json_file_with_logger
      cpkt_opcua_server_set_logger
      cpkt_opcua_client_new_with_logger
      cpkt_opcua_client_set_logger)
    if(NOT symbols MATCHES "(^|\n)[0-9a-fA-F]+[ \t]+T[ \t]+_?${symbol}(\n|$)")
      message(FATAL_ERROR "Missing public logging definition ${symbol}: ${library}")
    endif()
  endforeach()
endforeach()
