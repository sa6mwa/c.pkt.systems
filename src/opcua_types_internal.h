#ifndef CPKT_OPCUA_TYPES_INTERNAL_H
#define CPKT_OPCUA_TYPES_INTERNAL_H
#include <cpkt/opcua_types.h>
#include <open62541/client.h>
#include <open62541/server.h>
#include <open62541/types_generated.h>
#include <stddef.h>

typedef struct {
  size_t c_offset, ua_offset, c_size_offset, ua_size_offset;
  size_t type_index;
  int is_array, is_optional;
  const cpkt_opcua_Type *type;
} cpkt_opcua_type_member;
struct cpkt_opcua_Type {
  const char *name;
  size_t size;
  const UA_DataType *native;
  unsigned int kind;
  size_t members_size;
  const cpkt_opcua_type_member *members;
  size_t alignment;
  void *dynamic_owner;
};
/* Only generated code and the bridge include this file. No private bridge
 * symbol is externally linked: the generated table is included into the C TU.
 */
#endif
