#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <cpkt/opcua_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Evaluate each condition once. This also avoids glibc assert's repeated
 * macro arguments confusing clangd's expression-refactoring checks. */
static void check_condition(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(expression) check_condition(!!(expression), #expression, __LINE__)

#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
static void test_allocation_failures(void) {
  cpkt_opcua_WriteRequest request, copy;
  cpkt_opcua_WriteValue operation;
  cpkt_opcua_Argument argument;
  cpkt_opcua_ExtensionObject extension;
  cpkt_opcua_ByteString bytes, failure_bytes;
  size_t i;
  int injected;
  cpkt_opcua_StatusCode status;
  cpkt_opcua_WriteRequest_init(&request);
  cpkt_opcua_WriteValue_init(&operation);
  cpkt_opcua_Argument_init(&argument);
  cpkt_opcua_ExtensionObject_init(&extension);
  argument.name.length = 6;
  argument.name.data = (cpkt_opcua_Byte *)"nested";
  extension.encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
  extension.content.decoded.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ARGUMENT);
  extension.content.decoded.data = &argument;
  operation.value.hasValue = 1;
  operation.value.value.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EXTENSIONOBJECT);
  operation.value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  operation.value.value.data = &extension;
  request.nodesToWriteSize = 1;
  request.nodesToWrite = &operation;
  for (i = 0; i < 200; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_WriteRequest_copy(&request, &copy);
    injected = cpkt_types_fail_stop();
    if (injected) {
      CHECK(status != 0 && copy.nodesToWrite == NULL &&
            copy.nodesToWriteSize == 0);
    } else {
      CHECK(status == 0);
      cpkt_opcua_WriteRequest_clear(&copy);
      break;
    }
  }
  CHECK(i > 0 && i < 200);
  CHECK(cpkt_opcua_type_encode_binary(
            &request, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITEREQUEST),
            &bytes) == 0);
  for (i = 0; i < 200; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_type_decode_binary(
        &bytes, &copy, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITEREQUEST));
    injected = cpkt_types_fail_stop();
    if (injected) {
      CHECK(status != 0 && copy.nodesToWrite == NULL &&
            copy.nodesToWriteSize == 0);
    } else {
      CHECK(status == 0);
      cpkt_opcua_WriteRequest_clear(&copy);
      break;
    }
  }
  CHECK(i > 0 && i < 200);
  for (i = 0; i < 200; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_type_encode_binary(
        &request, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITEREQUEST),
        &failure_bytes);
    injected = cpkt_types_fail_stop();
    if (injected) {
      CHECK(status != 0 && failure_bytes.data == NULL &&
            failure_bytes.length == 0);
    } else {
      CHECK(status == 0);
      cpkt_opcua_ByteString_clear(&failure_bytes);
      break;
    }
  }
  CHECK(i > 0 && i < 200);
  cpkt_opcua_ByteString_clear(&bytes);
}
#endif

static void test_all_types(void) {
  size_t i, native_length;
  unsigned int expected, actual;
  unsigned char *native_bytes;
  cpkt_opcua_ByteString bytes;
  const cpkt_opcua_Type *type;
  void *value, *copy, *decoded;
  for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i) {
    type = cpkt_opcua_type_at(i);
    CHECK(type && cpkt_opcua_type_name(type) && cpkt_opcua_type_size(type));
    value = cpkt_opcua_type_new(type);
    copy = cpkt_opcua_type_new(type);
    decoded = cpkt_opcua_type_new(type);
    CHECK(value && copy && decoded);
    CHECK(cpkt_opcua_type_copy(value, copy, type) == 0);
    CHECK(cpkt_opcua_type_equal(value, copy, type));
    native_bytes = NULL;
    native_length = 0;
    expected = cpkt_types_native_empty(i, &native_bytes, &native_length);
    actual = cpkt_opcua_type_encode_binary(value, type, &bytes);
    CHECK(actual == expected);
    if (!actual) {
      CHECK(bytes.length == native_length);
      CHECK(!bytes.length ||
            memcmp(bytes.data, native_bytes, bytes.length) == 0);
      CHECK(cpkt_opcua_type_decode_binary(&bytes, decoded, type) == 0);
      CHECK(cpkt_opcua_type_equal(value, decoded, type));
    }
    cpkt_opcua_ByteString_clear(&bytes);
    cpkt_types_native_free(native_bytes);
    cpkt_opcua_type_delete(value, type);
    cpkt_opcua_type_delete(copy, type);
    cpkt_opcua_type_delete(decoded, type);
  }
  CHECK(cpkt_opcua_type_at(CPKT_OPCUA_TYPES_COUNT) == NULL);
}
static void test_populated_types(void) {
  size_t i, length;
  unsigned char *native;
  cpkt_opcua_ByteString input, output;
  const cpkt_opcua_Type *type;
  void *value, *copy;
  for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i) {
    native = NULL;
    length = 0;
    if (cpkt_types_native_populated(i, &native, &length) != 0) {
      fprintf(stderr, "Native fixture failed for type %s\n",
              cpkt_opcua_type_name(cpkt_opcua_type_at(i)));
      abort();
    }
    type = cpkt_opcua_type_at(i);
    value = cpkt_opcua_type_new(type);
    copy = cpkt_opcua_type_new(type);
    CHECK(value && copy);
    input.data = native;
    input.length = length;
    CHECK(cpkt_opcua_type_decode_binary(&input, value, type) == 0);
    CHECK(cpkt_opcua_type_copy(value, copy, type) == 0);
    CHECK(cpkt_opcua_type_equal(value, copy, type));
    CHECK(cpkt_opcua_type_encode_binary(copy, type, &output) == 0);
    if (output.length != length || memcmp(output.data, native, length) != 0) {
      fprintf(stderr, "Wire mismatch for %s: %lu / %lu bytes\n",
              cpkt_opcua_type_name(type), (unsigned long)output.length,
              (unsigned long)length);
      abort();
    }
    cpkt_opcua_ByteString_clear(&output);
    cpkt_types_native_free(native);
    cpkt_opcua_type_delete(value, type);
    cpkt_opcua_type_delete(copy, type);
  }
}
static void test_nested_values(void) {
  cpkt_opcua_WriteRequest request, copy, decoded;
  cpkt_opcua_WriteValue operation;
  cpkt_opcua_Int64 values[4];
  cpkt_opcua_UInt32 dimensions = 4;
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_ReadResponse response, response_copy;
  cpkt_opcua_DiagnosticInfo diagnostic, inner;
  cpkt_opcua_DataValue result;
  cpkt_opcua_ExtensionObject extension, extension_copy;
  cpkt_opcua_Argument argument;
  size_t i;
  cpkt_opcua_WriteRequest_init(&request);
  cpkt_opcua_WriteValue_init(&operation);
  values[0].high32 = 0x80000000U;
  values[0].low32 = 0;
  values[1].high32 = 0x7fffffffU;
  values[1].low32 = 0xffffffffU;
  values[2].high32 = 0xffffffffU;
  values[2].low32 = 0xffffffffU;
  values[3].high32 = 0;
  values[3].low32 = 0;
  request.nodesToWriteSize = 1;
  request.nodesToWrite = &operation;
  operation.nodeId.identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
  operation.nodeId.identifier.string.length = 5;
  operation.nodeId.identifier.string.data = (cpkt_opcua_Byte *)"a\0bcd";
  operation.attributeId = 13;
  operation.value.hasValue = 1;
  operation.value.hasStatus = 1;
  operation.value.hasSourceTimestamp = 1;
  operation.value.hasSourcePicoseconds = 1;
  operation.value.sourcePicoseconds = 9999;
  operation.value.sourceTimestamp.high32 = 0xffffffffU;
  operation.value.sourceTimestamp.low32 = 0xfffffffeU;
  operation.value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  operation.value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  operation.value.value.arrayLength = 4;
  operation.value.value.data = values;
  operation.value.value.arrayDimensionsSize = 1;
  operation.value.value.arrayDimensions = &dimensions;
  CHECK(cpkt_opcua_WriteRequest_copy(&request, &copy) == 0);
  CHECK(copy.nodesToWrite != &operation &&
        copy.nodesToWrite[0].value.value.data != values);
  CHECK(cpkt_opcua_WriteRequest_equal(&request, &copy));
  CHECK(cpkt_opcua_type_encode_binary(
            &request, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITEREQUEST),
            &bytes) == 0);
  CHECK(cpkt_opcua_type_decode_binary(
            &bytes, &decoded,
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITEREQUEST)) == 0);
  CHECK(cpkt_opcua_WriteRequest_equal(&request, &decoded));
  for (i = 0; i < 4; ++i) {
    cpkt_opcua_Int64 *v =
        (cpkt_opcua_Int64 *)decoded.nodesToWrite[0].value.value.data;
    CHECK(v[i].high32 == values[i].high32 && v[i].low32 == values[i].low32);
  }
  cpkt_opcua_WriteRequest_clear(&copy);
  cpkt_opcua_WriteRequest_clear(&decoded);
  cpkt_opcua_ByteString_clear(&bytes);
  cpkt_opcua_ReadResponse_init(&response);
  cpkt_opcua_DiagnosticInfo_init(&diagnostic);
  cpkt_opcua_DiagnosticInfo_init(&inner);
  cpkt_opcua_DataValue_init(&result);
  diagnostic.hasInnerDiagnosticInfo = 1;
  diagnostic.innerDiagnosticInfo = &inner;
  inner.hasAdditionalInfo = 1;
  inner.additionalInfo.length = 6;
  inner.additionalInfo.data = (cpkt_opcua_Byte *)"nested";
  response.diagnosticInfosSize = 1;
  response.diagnosticInfos = &diagnostic;
  result.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  result.value.data = CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
  result.hasValue = 1;
  response.resultsSize = 1;
  response.results = &result;
  CHECK(cpkt_opcua_ReadResponse_copy(&response, &response_copy) == 0);
  CHECK(response_copy.diagnosticInfos[0].innerDiagnosticInfo != &inner);
  CHECK(response_copy.results[0].value.data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  CHECK(cpkt_opcua_ReadResponse_equal(&response, &response_copy));
  cpkt_opcua_ReadResponse_clear(&response_copy);
  cpkt_opcua_Argument_init(&argument);
  cpkt_opcua_ExtensionObject_init(&extension);
  argument.name.length = 3;
  argument.name.data = (cpkt_opcua_Byte *)"arg";
  argument.dataType.identifier.numeric = 8;
  extension.encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
  extension.content.decoded.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ARGUMENT);
  extension.content.decoded.data = &argument;
  CHECK(cpkt_opcua_ExtensionObject_copy(&extension, &extension_copy) == 0);
  CHECK(extension_copy.encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED);
  CHECK(extension_copy.content.decoded.data != &argument);
  CHECK(cpkt_opcua_ExtensionObject_equal(&extension, &extension_copy));
  cpkt_opcua_ExtensionObject_clear(&extension_copy);
  cpkt_opcua_ExtensionObject_clear(&extension);
  CHECK(argument.name.length == 3);
  request.nodesToWriteSize = (size_t)-1;
  CHECK(cpkt_opcua_WriteRequest_copy(&request, &copy) != 0);
  CHECK(copy.nodesToWrite == NULL && copy.nodesToWriteSize == 0);
  request.nodesToWriteSize = 1;
  operation.value.value.type = NULL;
  CHECK(cpkt_opcua_WriteRequest_copy(&request, &copy) != 0);
}
static void test_failure_contracts(void) {
  cpkt_opcua_Boolean boolean = 254, boolean_copy;
  cpkt_opcua_ReadRequest request;
  cpkt_opcua_ByteString malformed;
  cpkt_opcua_Byte bad_byte = 0xff;
  cpkt_opcua_Variant borrowed, copy;
  cpkt_opcua_UInt64 scalar;
  cpkt_opcua_DiagnosticInfo chain[131], diagnostic_copy;
  size_t i;
  CHECK(cpkt_opcua_Boolean_copy(&boolean, &boolean_copy) == 0 &&
        boolean_copy == 1);
  malformed.data = &bad_byte;
  malformed.length = 1;
  CHECK(cpkt_opcua_type_decode_binary(
            &malformed, &request,
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_READREQUEST)) != 0);
  CHECK(request.nodesToRead == NULL && request.nodesToReadSize == 0);
  cpkt_opcua_Variant_init(&borrowed);
  scalar.high32 = 0xffffffffU;
  scalar.low32 = 0xffffffffU;
  borrowed.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT64);
  borrowed.data = &scalar;
  borrowed.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  CHECK(cpkt_opcua_Variant_copy(&borrowed, &copy) == 0);
  CHECK(copy.data != &scalar && copy.arrayLength == 0 &&
        copy.storageType == CPKT_OPCUA_VARIANT_DATA);
  cpkt_opcua_Variant_clear(&copy);
  cpkt_opcua_Variant_clear(&borrowed);
  CHECK(scalar.high32 == 0xffffffffU);
  cpkt_opcua_Variant_init(&borrowed);
  borrowed.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT64);
  CHECK(cpkt_opcua_Variant_copy(&borrowed, &copy) == 0 && copy.data == NULL);
  cpkt_opcua_Variant_clear(&copy);
  for (i = 0; i < 131; ++i) {
    cpkt_opcua_DiagnosticInfo_init(&chain[i]);
    if (i + 1 < 131) {
      chain[i].hasInnerDiagnosticInfo = 1;
      chain[i].innerDiagnosticInfo = &chain[i + 1];
    }
  }
  CHECK(cpkt_opcua_DiagnosticInfo_copy(&chain[0], &diagnostic_copy) != 0);
  CHECK(diagnostic_copy.innerDiagnosticInfo == NULL);
}

typedef struct {
  unsigned int calls;
  cpkt_opcua_UInt32 id;
  cpkt_opcua_StatusCode conversion, service;
  cpkt_opcua_ReadResponse copy;
} async_result;
static void read_complete(cpkt_opcua_client *client, void *user,
                          cpkt_opcua_UInt32 id, cpkt_opcua_StatusCode status,
                          const cpkt_opcua_ReadResponse *response) {
  async_result *result = (async_result *)user;
  CHECK(client != NULL);
  ++result->calls;
  result->id = id;
  result->conversion = status;
  if (status)
    CHECK(response == NULL);
  else {
    CHECK(response != NULL);
    result->service = response->responseHeader.serviceResult;
    CHECK(cpkt_opcua_ReadResponse_copy(response, &result->copy) == 0);
  }
}
static void generic_complete(cpkt_opcua_client *client, void *user,
                             cpkt_opcua_UInt32 id, cpkt_opcua_StatusCode status,
                             const void *response,
                             const cpkt_opcua_Type *type) {
  CHECK(type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_READRESPONSE));
  read_complete(client, user, id, status,
                (const cpkt_opcua_ReadResponse *)response);
}
static void test_async(cpkt_opcua_client *client,
                       cpkt_opcua_ReadRequest *read) {
  async_result results[3];
  cpkt_opcua_UInt32 ids[3], count;
  cpkt_opcua_client *disconnected = NULL;
  cpkt_opcua_ReadValueId saved[2];
  cpkt_opcua_Int64 *values;
  size_t i;
  memset(results, 0, sizeof(results));
  CHECK(cpkt_opcua_client_service_read_async(client, read, read_complete,
                                             &results[0], &ids[0]) == 0);
  CHECK(cpkt_opcua_client_service_async(
            client, read, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_READREQUEST),
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_READRESPONSE), generic_complete,
            &results[1], &ids[1]) == 0);
  CHECK(ids[0] && ids[1] && ids[0] != ids[1]);
  /* Submission encoded the request: the original nested storage is no longer
   * borrowed by either the facade or native async mechanism. */
  memcpy(saved, read->nodesToRead, sizeof(saved));
  memset(read->nodesToRead, 0, sizeof(saved));
  for (i = 0; i < 200 && (!results[0].calls || !results[1].calls); ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  memcpy(read->nodesToRead, saved, sizeof(saved));
  for (i = 0; i < 2; ++i) {
    CHECK(results[i].calls == 1 && results[i].id == ids[i] &&
          results[i].conversion == 0 && results[i].service == 0);
    CHECK(results[i].copy.resultsSize == 2 &&
          results[i].copy.results[0].value.arrayLength == 4 &&
          results[i].copy.results[1].status != 0);
    values = (cpkt_opcua_Int64 *)results[i].copy.results[0].value.data;
    CHECK(values[0].high32 == 0x80000000U && values[0].low32 == 0);
    CHECK(values[1].high32 == 0x7fffffffU && values[1].low32 == 0xffffffffU);
    cpkt_opcua_ReadResponse_clear(&results[i].copy);
  }
  ids[2] = 123;
  CHECK(cpkt_opcua_client_service_async(
            client, read, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_READREQUEST),
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITERESPONSE),
            generic_complete, &results[2], &ids[2]) != 0 &&
        ids[2] == 0);
  CHECK(cpkt_opcua_client_service_read_async(client, read, NULL, &results[2],
                                             &ids[2]) != 0);
  CHECK(cpkt_opcua_client_cancel_by_request_id(client, 0xffffffffU, &count) !=
        0);
  CHECK(count == 0 && results[2].calls == 0);
  CHECK(cpkt_opcua_client_new(&disconnected) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_client_service_read_async(disconnected, read, read_complete,
                                             &results[2], &ids[2]) != 0);
  CHECK(ids[2] == 0 && results[2].calls == 0);
  cpkt_opcua_client_free(disconnected);
  CHECK(cpkt_opcua_client_service_read_async(client, read, read_complete,
                                             &results[2], &ids[2]) == 0);
  cpkt_opcua_client_free(client);
  CHECK(results[2].calls == 1 && results[2].id == ids[2] &&
        results[2].conversion == 0);
  cpkt_opcua_ReadResponse_clear(&results[2].copy);
}

static void test_services(void) {
  unsigned short port;
  void *peer;
  char endpoint[64];
  cpkt_opcua_client *client = NULL;
  cpkt_opcua_WriteRequest request;
  cpkt_opcua_WriteResponse response;
  cpkt_opcua_WriteValue operation;
  cpkt_opcua_Int64 values[4];
  cpkt_opcua_ReadRequest read;
  cpkt_opcua_ReadResponse read_response;
  cpkt_opcua_ReadValueId nodes[2];
  cpkt_opcua_Int64 *received;
  cpkt_opcua_BrowseRequest browse;
  cpkt_opcua_BrowseResponse browse_response;
  cpkt_opcua_BrowseDescription description;
  size_t i;
  peer = cpkt_types_peer_start(&port);
  CHECK(peer);
  sprintf(endpoint, "opc.tcp://127.0.0.1:%u", (unsigned int)port);
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_client_connect(client, endpoint, NULL) == CPKT_OPCUA_OK);
  cpkt_opcua_WriteRequest_init(&request);
  cpkt_opcua_WriteValue_init(&operation);
  values[0].high32 = 0x80000000U;
  values[0].low32 = 0;
  values[1].high32 = 0x7fffffffU;
  values[1].low32 = 0xffffffffU;
  values[2].high32 = 0xffffffffU;
  values[2].low32 = 0xffffffffU;
  values[3].high32 = 0;
  values[3].low32 = 0;
  operation.nodeId.namespaceIndex = 1;
  operation.nodeId.identifier.numeric = 6001;
  operation.attributeId = 13;
  operation.value.hasValue = 1;
  operation.value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  operation.value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  operation.value.value.arrayLength = 4;
  operation.value.value.data = values;
  request.nodesToWriteSize = 1;
  request.nodesToWrite = &operation;
  CHECK(cpkt_opcua_client_service_write(client, &request, &response) == 0);
  CHECK(response.responseHeader.serviceResult == 0 &&
        response.resultsSize == 1 && response.results[0] == 0);
  cpkt_opcua_WriteResponse_clear(&response);
  CHECK(cpkt_types_peer_check(peer));
  cpkt_opcua_ReadRequest_init(&read);
  cpkt_opcua_ReadValueId_init(&nodes[0]);
  cpkt_opcua_ReadValueId_init(&nodes[1]);
  nodes[0].nodeId = operation.nodeId;
  nodes[0].attributeId = 13;
  nodes[1].nodeId.identifier.numeric = 0xffffffffU;
  nodes[1].attributeId = 13;
  read.nodesToReadSize = 2;
  read.nodesToRead = nodes;
  read.timestampsToReturn = cpkt_opcua_TIMESTAMPSTORETURN_BOTH;
  CHECK(cpkt_opcua_client_service_read(client, &read, &read_response) == 0);
  CHECK(read_response.responseHeader.serviceResult == 0 &&
        read_response.resultsSize == 2);
  CHECK(read_response.results[0].value.type ==
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  CHECK(read_response.results[0].value.arrayLength == 4);
  received = (cpkt_opcua_Int64 *)read_response.results[0].value.data;
  for (i = 0; i < 4; ++i)
    CHECK(received[i].high32 == values[i].high32 &&
          received[i].low32 == values[i].low32);
  CHECK(read_response.results[1].hasStatus &&
        read_response.results[1].status != 0);
  cpkt_opcua_ReadResponse_clear(&read_response);
  cpkt_opcua_BrowseRequest_init(&browse);
  cpkt_opcua_BrowseDescription_init(&description);
  description.nodeId.identifier.numeric = 85;
  description.browseDirection = cpkt_opcua_BROWSEDIRECTION_FORWARD;
  description.resultMask = 63;
  browse.nodesToBrowse = &description;
  browse.nodesToBrowseSize = 1;
  CHECK(cpkt_opcua_client_service_browse(client, &browse, &browse_response) ==
        0);
  CHECK(browse_response.responseHeader.serviceResult == 0 &&
        browse_response.resultsSize == 1);
  CHECK(browse_response.results[0].referencesSize > 0);
  cpkt_opcua_BrowseResponse_clear(&browse_response);
  CHECK(cpkt_opcua_client_service_read(NULL, &read, &read_response) != 0);
  cpkt_types_test_callbacks(client, peer);
  test_async(client, &read);
  cpkt_types_peer_stop(peer);
}
int main(void) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  test_allocation_failures();
#endif
  test_all_types();
  test_populated_types();
  test_nested_values();
  test_failure_contracts();
  cpkt_types_test_utilities();
  cpkt_types_test_identifiers();
  cpkt_types_test_plugins();
  cpkt_types_test_server();
  cpkt_types_test_nodes();
  cpkt_types_test_history_backend();
  test_services();
  puts("All public schema types, nested conversions and real typed services "
       "passed");
  return 0;
}
