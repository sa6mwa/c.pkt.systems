/* Handwritten OPC UA facade: files. */
#include "opcua_facade_internal.h"
#include <stdio.h>
#include <stdlib.h>

void cpkt_byte_string_clear_malloc(UA_ByteString *bytes) {
  if (bytes == NULL) {
    return;
  }
  free(bytes->data);
  bytes->data = NULL;
  bytes->length = 0;
}

cpkt_opcua_result cpkt_read_file_bytes(const char *path,
                                       UA_ByteString *bytes_out,
                                       cpkt_opcua_status *status_out) {
  FILE *file;
  long length;
  size_t read_count;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (bytes_out == NULL || path == NULL || path[0] == '\0') {
    return CPKT_OPCUA_ERR_ARG;
  }
  bytes_out->data = NULL;
  bytes_out->length = 0;
  file = fopen(path, "rb");
  if (file == NULL) {
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADNOTFOUND;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  length = ftell(file);
  if (length <= 0) {
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADCONFIGURATIONERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if ((unsigned long)length > (unsigned long)((size_t)-1)) {
    fclose(file);
    return CPKT_OPCUA_ERR_RANGE;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  bytes_out->data = (UA_Byte *)malloc((size_t)length);
  if (bytes_out->data == NULL) {
    fclose(file);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  read_count = fread(bytes_out->data, 1, (size_t)length, file);
  if (read_count != (size_t)length || ferror(file)) {
    cpkt_byte_string_clear_malloc(bytes_out);
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (fclose(file) != 0) {
    cpkt_byte_string_clear_malloc(bytes_out);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  bytes_out->length = (size_t)length;
  return CPKT_OPCUA_OK;
}
