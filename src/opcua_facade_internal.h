#ifndef CPKT_OPCUA_FACADE_INTERNAL_H
#define CPKT_OPCUA_FACADE_INTERNAL_H
#include "opcua_internal.h"
#include <cpkt/opcua_types.h>
#include <limits.h>
#include <open62541/client_subscriptions.h>
#include <open62541/util.h>
#include <stdarg.h>

/* Cross-module helpers are private on every supported GCC/Clang target.
 * Public facade declarations retain their default visibility. */
#define CPKT_OPCUA_PRIVATE __attribute__((visibility("hidden")))

#define CPKT_OPCUA_UINT32_MAX_VALUE 4294967295UL
#define CPKT_OPCUA_INT32_MIN_VALUE (-2147483647L - 1L)
#define CPKT_OPCUA_INT32_MAX_VALUE 2147483647L

typedef char cpkt_opcua_assert_upstream_uint64_is_64_bits
    [(sizeof(UA_UInt64) * CHAR_BIT == 64) ? 1 : -1];
typedef char cpkt_opcua_assert_upstream_datetime_is_64_bits
    [(sizeof(UA_DateTime) * CHAR_BIT == 64) ? 1 : -1];
typedef char cpkt_opcua_assert_upstream_status_is_32_bits
    [(sizeof(UA_StatusCode) * CHAR_BIT == 32) ? 1 : -1];

struct cpkt_opcua_monitor_context {
  struct cpkt_opcua_monitor_context *next;
  cpkt_opcua_client *owner;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  cpkt_opcua_data_change_fn fn;
  cpkt_opcua_event_fn event_fn;
  cpkt_opcua_event_fields_fn event_fields_fn;
  char **event_field_names;
  size_t event_field_count;
  void *user;
};

struct cpkt_opcua_async_context {
  struct cpkt_opcua_async_context *next;
  cpkt_opcua_client *owner;
  cpkt_opcua_async_value_fn value_fn;
  cpkt_opcua_async_status_fn status_fn;
  cpkt_opcua_browse_fn browse_fn;
  cpkt_opcua_async_browse_fn browse_done_fn;
  cpkt_opcua_async_call_fn call_fn;
  cpkt_opcua_async_node_fn node_fn;
  void *user;
  cpkt_opcua_value *outputs;
  size_t expected_output_count;
  char *string_buffer;
  size_t string_buffer_size;
  size_t *required_string_size_out;
  char **string_buffers;
  const size_t *string_buffer_sizes;
  size_t *required_string_sizes_out;
  cpkt_opcua_node_id *node_id_out;
  char *node_id_buffer;
  size_t node_id_buffer_size;
  size_t *required_node_id_size_out;
};

struct cpkt_opcua_method_context {
  struct cpkt_opcua_method_context *next;
  cpkt_opcua_method_many_fn fn;
  cpkt_opcua_method_fn single_fn;
  void *user;
  size_t input_count;
  size_t output_count;
  int *input_types;
  int *output_types;
};

struct cpkt_owned_node_id_memory {
  char *string;
  unsigned char *byte_string;
};

CPKT_OPCUA_PRIVATE void cpkt_byte_string_clear_malloc(UA_ByteString *bytes);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_read_file_bytes(
    const char *path, UA_ByteString *bytes_out, cpkt_opcua_status *status_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_status cpkt_status(UA_StatusCode status);
CPKT_OPCUA_PRIVATE int cpkt_valid_uint64_words(unsigned long high32,
                                               unsigned long low32);
CPKT_OPCUA_PRIVATE int cpkt_valid_datetime_words(long high32,
                                                 unsigned long low32);
CPKT_OPCUA_PRIVATE UA_UInt64 cpkt_make_uint64(cpkt_opcua_uint64 value);
CPKT_OPCUA_PRIVATE UA_DateTime cpkt_make_datetime(cpkt_opcua_datetime value);
CPKT_OPCUA_PRIVATE cpkt_opcua_uint64 cpkt_uint64_from_native(UA_UInt64 value);
CPKT_OPCUA_PRIVATE cpkt_opcua_datetime
cpkt_datetime_from_native(UA_DateTime value);
CPKT_OPCUA_PRIVATE UA_Guid cpkt_make_guid(const unsigned char guid[16]);
CPKT_OPCUA_PRIVATE void cpkt_guid_from_native(UA_Guid native_guid,
                                              unsigned char guid[16]);
CPKT_OPCUA_PRIVATE UA_NodeId cpkt_make_node_id(cpkt_opcua_node_id node_id);
CPKT_OPCUA_PRIVATE UA_ExpandedNodeId
cpkt_make_expanded_node_id(cpkt_opcua_expanded_node_id node_id);
CPKT_OPCUA_PRIVATE UA_ByteString
cpkt_make_borrowed_byte_string(const unsigned char *data, size_t length);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_make_borrowed_byte_string_array(
    const cpkt_opcua_byte_string_view *views, size_t view_count,
    UA_ByteString **bytes_out);
CPKT_OPCUA_PRIVATE int cpkt_valid_node_id(cpkt_opcua_node_id node_id);
CPKT_OPCUA_PRIVATE int
cpkt_valid_expanded_node_id(cpkt_opcua_expanded_node_id node_id);
CPKT_OPCUA_PRIVATE int cpkt_valid_node_class(unsigned long node_class);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_ua_byte_string_to_buffer(
    const UA_ByteString *value, unsigned char *buffer, size_t buffer_size,
    size_t *required_size_out);
CPKT_OPCUA_PRIVATE int
cpkt_native_node_id_to_facade(const UA_NodeId *native, cpkt_opcua_node_id *out,
                              struct cpkt_owned_node_id_memory *owned_out);
CPKT_OPCUA_PRIVATE void
cpkt_owned_node_id_memory_clear(struct cpkt_owned_node_id_memory *owned);
CPKT_OPCUA_PRIVATE char *cpkt_copy_ua_string(const UA_String *value);
CPKT_OPCUA_PRIVATE char *cpkt_strdup_c89(const char *value);
CPKT_OPCUA_PRIVATE unsigned char *cpkt_memdup_c89(const unsigned char *value,
                                                  size_t length);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_copy_facade_node_id(cpkt_opcua_node_id in, cpkt_opcua_node_id *out,
                         struct cpkt_owned_node_id_memory *owned_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_native_node_id_to_output(
    const UA_NodeId *native, cpkt_opcua_node_id *node_id_out, char *buffer,
    size_t buffer_size, size_t *required_size_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_copy_ua_string_to_buffer(const UA_String *value, char *buffer,
                              size_t buffer_size, size_t *required_size_out);
CPKT_OPCUA_PRIVATE int cpkt_ulong_fits_uint32(unsigned long value);
CPKT_OPCUA_PRIVATE int cpkt_long_fits_int32(long value);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_array_dimensions_to_buffer(
    const UA_UInt32 *native_dimensions, size_t native_dimension_count,
    unsigned long *dimensions, size_t dimension_count,
    size_t *required_dimension_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_make_array_dimensions(
    const unsigned long *dimensions, size_t dimension_count,
    UA_UInt32 **native_dimensions_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_apply_value_shape_to_variable_attributes(UA_VariableAttributes *attr,
                                              const cpkt_opcua_value *value);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_apply_value_shape_to_variable_type_attributes(
    UA_VariableTypeAttributes *attr, const cpkt_opcua_value *value);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_copy_node_id_to_caller(const cpkt_opcua_node_id *source,
                            const struct cpkt_owned_node_id_memory *owned,
                            cpkt_opcua_node_id *target_out, char *buffer,
                            size_t buffer_size, size_t *required_size_out);
CPKT_OPCUA_PRIVATE UA_NodeId cpkt_data_type_node_id_for_value_type(int type);
CPKT_OPCUA_PRIVATE int cpkt_valid_method_value_type(int type);
CPKT_OPCUA_PRIVATE int cpkt_value_type_needs_buffer(int type);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_browse_result_each(UA_BrowseResult *browse_result, cpkt_opcua_browse_fn fn,
                        void *user, cpkt_opcua_status *status_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_fill_browse_description(
    UA_BrowseDescription *browse_description, cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_fill_browse_path(
    UA_BrowsePath *browse_path, cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements, size_t element_count);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_set_variant(UA_Variant *variant, const cpkt_opcua_value *value);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_set_variant_array(
    UA_Variant *variants, const cpkt_opcua_value *values, size_t value_count);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_get_variant(
    const UA_Variant *variant, cpkt_opcua_value *value_out, char *string_buffer,
    size_t string_buffer_size, size_t *required_string_size_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_get_variant_borrowed(
    const UA_Variant *variant, cpkt_opcua_value *value_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_get_data_value(
    const UA_DataValue *data_value, cpkt_opcua_data_value *data_value_out,
    char *string_buffer, size_t string_buffer_size,
    size_t *required_string_size_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_boolean_array_from_variant(
    const UA_Variant *variant, int *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_integer_array_from_variant(
    const UA_Variant *variant, long *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_uint64_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_uint64 *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_datetime_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_datetime *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_status_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_status *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_guid_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_guid *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_copy_double_array_from_variant(
    const UA_Variant *variant, double *values, size_t value_count,
    size_t *required_value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_each_string_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_string_array_fn fn, void *user,
    size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_each_byte_string_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_byte_string_array_fn fn, void *user,
    size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_each_qualified_name_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_qualified_name_array_fn fn,
    void *user, size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_each_qualified_name_range_from_variant(
    const UA_Variant *variant, cpkt_opcua_qualified_name_array_fn fn,
    void *user, size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_each_localized_text_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_localized_text_array_fn fn,
    void *user, size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_each_localized_text_range_from_variant(
    const UA_Variant *variant, cpkt_opcua_localized_text_array_fn fn,
    void *user, size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_each_string_range_from_variant(
    const UA_Variant *variant, cpkt_opcua_string_array_fn fn, void *user,
    size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_each_byte_string_range_from_variant(
    const UA_Variant *variant, cpkt_opcua_byte_string_array_fn fn, void *user,
    size_t *value_count_out);
CPKT_OPCUA_PRIVATE int
cpkt_parse_simple_array_index_range(const char *index_range, size_t *start_out,
                                    size_t *count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result cpkt_each_string_array_slice_from_variant(
    const UA_Variant *variant, size_t start, size_t count,
    cpkt_opcua_string_array_fn fn, void *user, size_t *value_count_out);
CPKT_OPCUA_PRIVATE cpkt_opcua_result
cpkt_each_byte_string_array_slice_from_variant(
    const UA_Variant *variant, size_t start, size_t count,
    cpkt_opcua_byte_string_array_fn fn, void *user, size_t *value_count_out);
CPKT_OPCUA_PRIVATE int cpkt_logger_valid(const cpkt_opcua_log_config *config);
CPKT_OPCUA_PRIVATE void cpkt_logger_set(struct cpkt_opcua_logger *logger,
                                        UA_Logger *native,
                                        const cpkt_opcua_log_config *config);

CPKT_OPCUA_PRIVATE void cpkt_logger_log(void *, UA_LogLevel, UA_LogCategory,
                                        const char *, va_list);

#endif
