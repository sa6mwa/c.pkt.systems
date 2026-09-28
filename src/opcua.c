/* Handwritten OPC UA facade: core. */
#include "opcua_facade_internal.h"

#ifndef CPKT_OPCUA_FACADE_VERSION
#define CPKT_OPCUA_FACADE_VERSION "0"
#endif

/** Returns the linked open62541 version string. */
const char *cpkt_opcua_open62541_version(void) { return UA_OPEN62541_VERSION; }

/** Returns the public OPC UA facade ABI version string. */
const char *cpkt_opcua_facade_version(void) {
  return CPKT_OPCUA_FACADE_VERSION;
}

/** Converts an upstream status code into its stable diagnostic name. */
const char *cpkt_opcua_status_name(cpkt_opcua_status status) {
  return UA_StatusCode_name((UA_StatusCode)status);
}

/** Converts an OPC UA facade result code into a stable diagnostic string. */
const char *cpkt_opcua_result_string(cpkt_opcua_result result) {
  switch (result) {
  case CPKT_OPCUA_OK:
    return "ok";
  case CPKT_OPCUA_ERR_ARG:
    return "invalid argument";
  case CPKT_OPCUA_ERR_ALLOC:
    return "allocation failed";
  case CPKT_OPCUA_ERR_UPSTREAM:
    return "open62541 operation failed";
  case CPKT_OPCUA_ERR_TYPE:
    return "unsupported value type";
  case CPKT_OPCUA_ERR_RANGE:
    return "value or buffer out of range";
  case CPKT_OPCUA_ERR_CALLBACK:
    return "callback failed";
  default:
    return "unknown result";
  }
}
