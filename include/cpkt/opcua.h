#ifndef CPKT_OPCUA_H
#define CPKT_OPCUA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpkt_opcua_client cpkt_opcua_client;
typedef struct cpkt_opcua_server cpkt_opcua_server;
typedef struct cpkt_opcua_server_event cpkt_opcua_server_event;

/* Upstream status code carried without exposing upstream integer typedefs. */
typedef unsigned long cpkt_opcua_status;
/* Subscription and monitored-item ids are owned by the connected client. */
typedef unsigned long cpkt_opcua_subscription_id;
typedef unsigned long cpkt_opcua_monitored_item_id;
/* Async request ids can be used with native cancellation APIs. */
typedef unsigned long cpkt_opcua_request_id;

/* Facade operation result. Check status_out for upstream failures. */
typedef enum cpkt_opcua_result {
  CPKT_OPCUA_OK = 0,
  CPKT_OPCUA_ERR_ARG = 1,
  CPKT_OPCUA_ERR_ALLOC = 2,
  CPKT_OPCUA_ERR_UPSTREAM = 3,
  CPKT_OPCUA_ERR_TYPE = 4,
  CPKT_OPCUA_ERR_RANGE = 5,
  CPKT_OPCUA_ERR_CALLBACK = 6
} cpkt_opcua_result;

/* Node id kind supported by the C89 facade. */
typedef enum cpkt_opcua_node_id_type {
  CPKT_OPCUA_NODE_ID_NULL = 0,
  CPKT_OPCUA_NODE_ID_NUMERIC = 1,
  CPKT_OPCUA_NODE_ID_STRING = 2,
  CPKT_OPCUA_NODE_ID_GUID = 3,
  CPKT_OPCUA_NODE_ID_BYTE_STRING = 4
} cpkt_opcua_node_id_type;

/* Numeric, string, GUID, or byte-string node id. Pointers are borrowed by the call. */
typedef struct cpkt_opcua_node_id {
  unsigned short namespace_index;
  int identifier_type;
  unsigned long numeric;
  const char *string;
  unsigned char guid[16];
  const unsigned char *byte_string;
  size_t byte_string_length;
} cpkt_opcua_node_id;

/* Expanded node id. The optional namespace URI pointer is borrowed by the call. */
typedef struct cpkt_opcua_expanded_node_id {
  cpkt_opcua_node_id node_id;
  const char *namespace_uri;
  size_t namespace_uri_length;
  unsigned long server_index;
} cpkt_opcua_expanded_node_id;

/* Scalar value kinds supported by the C89 facade. */
typedef enum cpkt_opcua_value_type {
  CPKT_OPCUA_VALUE_EMPTY = 0,
  CPKT_OPCUA_VALUE_BOOLEAN = 1,
  CPKT_OPCUA_VALUE_INTEGER = 2,
  CPKT_OPCUA_VALUE_DOUBLE = 3,
  CPKT_OPCUA_VALUE_STRING = 4,
  CPKT_OPCUA_VALUE_BYTE_STRING = 5,
  CPKT_OPCUA_VALUE_BOOLEAN_ARRAY = 6,
  CPKT_OPCUA_VALUE_INTEGER_ARRAY = 7,
  CPKT_OPCUA_VALUE_DOUBLE_ARRAY = 8,
  CPKT_OPCUA_VALUE_STRING_ARRAY = 9,
  CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY = 10,
  CPKT_OPCUA_VALUE_GUID = 11,
  CPKT_OPCUA_VALUE_STATUS = 12,
  CPKT_OPCUA_VALUE_QUALIFIED_NAME = 13,
  CPKT_OPCUA_VALUE_LOCALIZED_TEXT = 14,
  CPKT_OPCUA_VALUE_UINT64 = 15,
  CPKT_OPCUA_VALUE_DATETIME = 16,
  CPKT_OPCUA_VALUE_UINT64_ARRAY = 17,
  CPKT_OPCUA_VALUE_DATETIME_ARRAY = 18,
  CPKT_OPCUA_VALUE_STATUS_ARRAY = 19,
  CPKT_OPCUA_VALUE_GUID_ARRAY = 20,
  CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY = 21,
  CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY = 22
} cpkt_opcua_value_type;

typedef enum cpkt_opcua_standard_data_type {
  CPKT_OPCUA_STANDARD_DATA_TYPE_BOOLEAN = 1,
  CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER = 6,
  CPKT_OPCUA_STANDARD_DATA_TYPE_UINT64 = 9,
  CPKT_OPCUA_STANDARD_DATA_TYPE_DOUBLE = 11,
  CPKT_OPCUA_STANDARD_DATA_TYPE_DATETIME = 13,
  CPKT_OPCUA_STANDARD_DATA_TYPE_STRING = 12,
  CPKT_OPCUA_STANDARD_DATA_TYPE_BYTE_STRING = 15,
  CPKT_OPCUA_STANDARD_DATA_TYPE_GUID = 14,
  CPKT_OPCUA_STANDARD_DATA_TYPE_STATUS = 19,
  CPKT_OPCUA_STANDARD_DATA_TYPE_QUALIFIED_NAME = 20,
  CPKT_OPCUA_STANDARD_DATA_TYPE_LOCALIZED_TEXT = 21
} cpkt_opcua_standard_data_type;

/* OPC UA node class values represented without upstream headers. */
typedef enum cpkt_opcua_node_class {
  CPKT_OPCUA_NODE_CLASS_UNSPECIFIED = 0,
  CPKT_OPCUA_NODE_CLASS_OBJECT = 1,
  CPKT_OPCUA_NODE_CLASS_VARIABLE = 2,
  CPKT_OPCUA_NODE_CLASS_METHOD = 4,
  CPKT_OPCUA_NODE_CLASS_OBJECT_TYPE = 8,
  CPKT_OPCUA_NODE_CLASS_VARIABLE_TYPE = 16,
  CPKT_OPCUA_NODE_CLASS_REFERENCE_TYPE = 32,
  CPKT_OPCUA_NODE_CLASS_DATA_TYPE = 64,
  CPKT_OPCUA_NODE_CLASS_VIEW = 128
} cpkt_opcua_node_class;

typedef enum cpkt_opcua_browse_direction {
  CPKT_OPCUA_BROWSE_FORWARD = 0,
  CPKT_OPCUA_BROWSE_INVERSE = 1,
  CPKT_OPCUA_BROWSE_BOTH = 2
} cpkt_opcua_browse_direction;

typedef enum cpkt_opcua_browse_result_mask {
  CPKT_OPCUA_BROWSE_RESULT_REFERENCE_TYPE = 1,
  CPKT_OPCUA_BROWSE_RESULT_IS_FORWARD = 2,
  CPKT_OPCUA_BROWSE_RESULT_NODE_CLASS = 4,
  CPKT_OPCUA_BROWSE_RESULT_BROWSE_NAME = 8,
  CPKT_OPCUA_BROWSE_RESULT_DISPLAY_NAME = 16,
  CPKT_OPCUA_BROWSE_RESULT_TYPE_DEFINITION = 32,
  CPKT_OPCUA_BROWSE_RESULT_ALL = 63
} cpkt_opcua_browse_result_mask;

typedef enum cpkt_opcua_monitoring_mode {
  CPKT_OPCUA_MONITORING_DISABLED = 0,
  CPKT_OPCUA_MONITORING_SAMPLING = 1,
  CPKT_OPCUA_MONITORING_REPORTING = 2
} cpkt_opcua_monitoring_mode;

typedef enum cpkt_opcua_deadband_type {
  CPKT_OPCUA_DEADBAND_NONE = 0,
  CPKT_OPCUA_DEADBAND_ABSOLUTE = 1,
  CPKT_OPCUA_DEADBAND_PERCENT = 2
} cpkt_opcua_deadband_type;

typedef enum cpkt_opcua_method_argument_direction {
  CPKT_OPCUA_METHOD_ARGUMENT_INPUT = 0,
  CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT = 1
} cpkt_opcua_method_argument_direction;

typedef struct cpkt_opcua_browse_options {
  int browse_direction;
  int include_subtypes;
  int has_reference_type;
  cpkt_opcua_node_id reference_type_id;
  unsigned long node_class_mask;
  unsigned long result_mask;
  unsigned long max_references;
} cpkt_opcua_browse_options;

typedef struct cpkt_opcua_monitor_options {
  double sampling_interval_ms;
  unsigned long queue_size;
  int discard_oldest;
  int deadband_type;
  double deadband_value;
} cpkt_opcua_monitor_options;

typedef struct cpkt_opcua_mqtt_connection_options {
  const char *name;
  const char *broker_host;
  unsigned short broker_port;
  const char *topic;
  int subscribe;
  const char *username;
  const char *password;
  unsigned long publisher_id;
  unsigned short keep_alive_seconds;
  int validate_only;
  int enabled;
} cpkt_opcua_mqtt_connection_options;

typedef struct cpkt_opcua_pubsub_writer_group_options {
  const char *name;
  unsigned short writer_group_id;
  double publishing_interval_ms;
  int json_encoding;
  int enabled;
} cpkt_opcua_pubsub_writer_group_options;

typedef struct cpkt_opcua_pubsub_data_set_writer_options {
  const char *name;
  unsigned short data_set_writer_id;
  unsigned long key_frame_count;
  int enabled;
} cpkt_opcua_pubsub_data_set_writer_options;

typedef struct cpkt_opcua_pubsub_reader_group_options {
  const char *name;
  int json_encoding;
  int enabled;
} cpkt_opcua_pubsub_reader_group_options;

typedef struct cpkt_opcua_pubsub_data_set_reader_options {
  const char *name;
  unsigned long publisher_id;
  unsigned short writer_group_id;
  unsigned short data_set_writer_id;
  double message_receive_timeout_ms;
  int enabled;
} cpkt_opcua_pubsub_data_set_reader_options;

typedef struct cpkt_opcua_string_view {
  const char *data;
  size_t length;
} cpkt_opcua_string_view;

typedef struct cpkt_opcua_byte_string_view {
  const unsigned char *data;
  size_t length;
} cpkt_opcua_byte_string_view;

typedef struct cpkt_opcua_guid {
  unsigned char bytes[16];
} cpkt_opcua_guid;

typedef struct cpkt_opcua_qualified_name_view {
  unsigned short namespace_index;
  const char *name;
  size_t name_length;
} cpkt_opcua_qualified_name_view;

typedef struct cpkt_opcua_localized_text_view {
  const char *locale;
  size_t locale_length;
  const char *text;
  size_t text_length;
} cpkt_opcua_localized_text_view;

/* C89-safe UInt64 storage. Each word must fit in 32 bits. */
typedef struct cpkt_opcua_uint64 {
  unsigned long high32;
  unsigned long low32;
} cpkt_opcua_uint64;

/* C89-safe DateTime storage. high32 is signed; low32 must fit in 32 bits. */
typedef struct cpkt_opcua_datetime {
  long high32;
  unsigned long low32;
} cpkt_opcua_datetime;

typedef struct cpkt_opcua_event {
  const unsigned char *event_id;
  size_t event_id_length;
  const char *source_name;
  size_t source_name_length;
  const char *message;
  size_t message_length;
  unsigned long severity;
} cpkt_opcua_event;

/*
 * Scalar value container. String and byte-string values point at caller-owned
 * memory on input and at the caller's read buffer on output.
 */
typedef struct cpkt_opcua_value {
  int type;
  int boolean_value;
  long integer_value;
  double double_value;
  const char *string_value;
  size_t string_length;
  const unsigned char *bytes_value;
  size_t bytes_length;
  const int *boolean_array_values;
  size_t boolean_array_length;
  const long *integer_array_values;
  size_t integer_array_length;
  const double *double_array_values;
  size_t double_array_length;
  const cpkt_opcua_string_view *string_array_values;
  size_t string_array_length;
  const cpkt_opcua_byte_string_view *byte_string_array_values;
  size_t byte_string_array_length;
  const cpkt_opcua_uint64 *uint64_array_values;
  size_t uint64_array_length;
  const cpkt_opcua_datetime *datetime_array_values;
  size_t datetime_array_length;
  const cpkt_opcua_status *status_array_values;
  size_t status_array_length;
  const cpkt_opcua_guid *guid_array_values;
  size_t guid_array_length;
  const cpkt_opcua_qualified_name_view *qualified_name_array_values;
  size_t qualified_name_array_length;
  const cpkt_opcua_localized_text_view *localized_text_array_values;
  size_t localized_text_array_length;
  unsigned char guid_value[16];
  cpkt_opcua_status status_value;
  unsigned short qualified_name_namespace_index;
  const char *qualified_name;
  size_t qualified_name_length;
  const char *localized_text_locale;
  size_t localized_text_locale_length;
  const char *localized_text;
  size_t localized_text_length;
  cpkt_opcua_uint64 uint64_value;
  cpkt_opcua_datetime datetime_value;
} cpkt_opcua_value;

typedef struct cpkt_opcua_event_field {
  const char *name;
  size_t name_length;
  cpkt_opcua_value value;
  cpkt_opcua_status status;
} cpkt_opcua_event_field;

/*
 * C89 data-value container. String and byte-string values point at the caller's
 * read buffer on output. Use native data-value callbacks for generated fields
 * not represented here.
 */
typedef struct cpkt_opcua_data_value {
  int has_value;
  cpkt_opcua_value value;
  int has_status;
  cpkt_opcua_status status;
  int has_source_timestamp;
  cpkt_opcua_datetime source_timestamp;
  int has_server_timestamp;
  cpkt_opcua_datetime server_timestamp;
} cpkt_opcua_data_value;

/*
 * Browse callback entry. String fields and target string node ids are valid
 * only for the duration of the callback.
 */
typedef struct cpkt_opcua_browse_entry {
  cpkt_opcua_node_id target_node_id;
  unsigned long node_class;
  unsigned short browse_name_namespace_index;
  const char *browse_name;
  const char *display_name;
  int is_forward;
} cpkt_opcua_browse_entry;

/*
 * One hierarchical browse-path component. Browse name strings are borrowed by
 * the call.
 */
typedef struct cpkt_opcua_browse_path_element {
  unsigned short namespace_index;
  const char *browse_name;
} cpkt_opcua_browse_path_element;

/* Native callbacks receive borrowed upstream client/server pointers. */
typedef cpkt_opcua_status (*cpkt_opcua_client_native_fn)(void *native_client, void *user);
typedef cpkt_opcua_status (*cpkt_opcua_server_native_fn)(void *native_server, void *user);
/* Native config callbacks receive borrowed upstream config pointers. */
typedef cpkt_opcua_status (*cpkt_opcua_client_native_config_fn)(void *native_client_config, void *user);
typedef cpkt_opcua_status (*cpkt_opcua_server_native_config_fn)(void *native_server_config, void *user);
/* Return zero/GOOD to accept a username/password session, or an upstream status to reject it. */
typedef cpkt_opcua_status (*cpkt_opcua_login_fn)(
    const char *username,
    size_t username_length,
    const unsigned char *password,
    size_t password_length,
    void *user);
/* Native value callbacks receive borrowed upstream value pointers. */
typedef int (*cpkt_opcua_native_variant_fn)(const void *native_variant, void *user);
typedef int (*cpkt_opcua_native_data_value_fn)(const void *native_data_value, void *user);
/* Return zero to continue history iteration; nonzero stops with callback error. */
typedef int (*cpkt_opcua_history_data_value_fn)(
    const cpkt_opcua_data_value *data_value,
    int more_data_available,
    void *user);
/* Return zero to continue browsing; nonzero stops with callback error. */
typedef int (*cpkt_opcua_browse_fn)(const cpkt_opcua_browse_entry *entry, void *user);
typedef int (*cpkt_opcua_string_array_fn)(size_t index, const char *data, size_t length, void *user);
typedef int (*cpkt_opcua_byte_string_array_fn)(
    size_t index,
    const unsigned char *data,
    size_t length,
    void *user);
typedef int (*cpkt_opcua_qualified_name_array_fn)(
    size_t index,
    unsigned short namespace_index,
    const char *name,
    size_t name_length,
    void *user);
typedef int (*cpkt_opcua_localized_text_array_fn)(
    size_t index,
    const char *locale,
    size_t locale_length,
    const char *text,
    size_t text_length,
    void *user);
/* Value callback data is borrowed and only valid until the callback returns. */
typedef void (*cpkt_opcua_data_change_fn)(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_status status,
    void *user);
typedef void (*cpkt_opcua_event_fn)(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_event *event,
    cpkt_opcua_status status,
    void *user);
typedef void (*cpkt_opcua_event_fields_fn)(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_event_field *fields,
    size_t field_count,
    cpkt_opcua_status status,
    void *user);
/* Async callback values are borrowed and only valid until the callback returns. */
typedef void (*cpkt_opcua_async_value_fn)(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    const cpkt_opcua_value *value,
    cpkt_opcua_status status,
    void *user);
typedef void (*cpkt_opcua_async_status_fn)(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    cpkt_opcua_status status,
    void *user);
typedef void (*cpkt_opcua_async_browse_fn)(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    cpkt_opcua_status status,
    void *user);
typedef void (*cpkt_opcua_async_call_fn)(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    const cpkt_opcua_value *outputs,
    size_t output_count,
    cpkt_opcua_status status,
    void *user);
typedef void (*cpkt_opcua_async_node_fn)(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    const cpkt_opcua_node_id *node_id,
    cpkt_opcua_status status,
    void *user);
/*
 * Method callback input values are borrowed. Output storage is owned by the
 * caller and must be filled with one scalar value before returning zero.
 */
typedef cpkt_opcua_result (*cpkt_opcua_method_fn)(
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *output,
    void *user);
typedef cpkt_opcua_result (*cpkt_opcua_method_many_fn)(
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *outputs,
    size_t output_count,
    void *user);

/* Return bundled upstream and facade ABI versions as static strings. */
const char *cpkt_opcua_open62541_version(void);
const char *cpkt_opcua_facade_version(void);
/* Return static diagnostic text for statuses and facade results. */
const char *cpkt_opcua_status_name(cpkt_opcua_status status);
const char *cpkt_opcua_result_string(cpkt_opcua_result result);

/* Construct C89-safe node id values. */
cpkt_opcua_node_id cpkt_opcua_node_id_null(void);
cpkt_opcua_node_id cpkt_opcua_node_id_numeric(
    unsigned short namespace_index,
    unsigned long identifier);
cpkt_opcua_node_id cpkt_opcua_node_id_string(
    unsigned short namespace_index,
    const char *identifier);
cpkt_opcua_node_id cpkt_opcua_node_id_guid(
    unsigned short namespace_index,
    const unsigned char guid[16]);
cpkt_opcua_node_id cpkt_opcua_node_id_byte_string(
    unsigned short namespace_index,
    const unsigned char *identifier,
    size_t identifier_length);
int cpkt_opcua_node_id_equal(cpkt_opcua_node_id a, cpkt_opcua_node_id b);
cpkt_opcua_result cpkt_opcua_node_id_print(
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_node_id_parse(
    const char *text,
    cpkt_opcua_node_id *node_id_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_local(cpkt_opcua_node_id node_id);
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_uri(
    const char *namespace_uri,
    size_t namespace_uri_length,
    cpkt_opcua_node_id node_id);
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_server(
    unsigned long server_index,
    cpkt_opcua_node_id node_id);
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_server_uri(
    unsigned long server_index,
    const char *namespace_uri,
    size_t namespace_uri_length,
    cpkt_opcua_node_id node_id);
int cpkt_opcua_expanded_node_id_equal(
    cpkt_opcua_expanded_node_id a,
    cpkt_opcua_expanded_node_id b);
cpkt_opcua_result cpkt_opcua_expanded_node_id_print(
    cpkt_opcua_expanded_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_expanded_node_id_parse(
    const char *text,
    cpkt_opcua_expanded_node_id *node_id_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
/* Print and parse C89-safe value helper types. Parsed strings point into caller storage. */
cpkt_opcua_result cpkt_opcua_guid_print(
    const unsigned char guid[16],
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_guid_parse(
    const char *text,
    unsigned char guid_out[16]);
cpkt_opcua_result cpkt_opcua_qualified_name_print(
    unsigned short namespace_index,
    const char *name,
    size_t name_length,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_qualified_name_parse(
    const char *text,
    unsigned short *namespace_index_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *name_length_out,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_localized_text_print(
    const char *locale,
    size_t locale_length,
    const char *text,
    size_t text_length,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_localized_text_parse(
    const char *input,
    char *buffer,
    size_t buffer_size,
    const char **locale_out,
    size_t *locale_length_out,
    const char **text_out,
    size_t *text_length_out,
    size_t *required_size_out);

/* Initialize scalar values. These functions do not allocate. */
void cpkt_opcua_value_clear(cpkt_opcua_value *value);
void cpkt_opcua_value_boolean(cpkt_opcua_value *value, int boolean_value);
void cpkt_opcua_value_integer(cpkt_opcua_value *value, long integer_value);
void cpkt_opcua_value_double(cpkt_opcua_value *value, double double_value);
void cpkt_opcua_value_string(
    cpkt_opcua_value *value,
    const char *string_value,
    size_t string_length);
void cpkt_opcua_value_byte_string(
    cpkt_opcua_value *value,
    const unsigned char *bytes_value,
    size_t bytes_length);
void cpkt_opcua_value_guid(
    cpkt_opcua_value *value,
    const unsigned char guid[16]);
void cpkt_opcua_value_status(
    cpkt_opcua_value *value,
    cpkt_opcua_status status_value);
void cpkt_opcua_value_qualified_name(
    cpkt_opcua_value *value,
    unsigned short namespace_index,
    const char *name,
    size_t name_length);
void cpkt_opcua_value_localized_text(
    cpkt_opcua_value *value,
    const char *locale,
    size_t locale_length,
    const char *text,
    size_t text_length);
void cpkt_opcua_value_uint64(
    cpkt_opcua_value *value,
    unsigned long high32,
    unsigned long low32);
void cpkt_opcua_value_datetime(
    cpkt_opcua_value *value,
    long high32,
    unsigned long low32);
void cpkt_opcua_value_boolean_array(
    cpkt_opcua_value *value,
    const int *values,
    size_t value_count);
void cpkt_opcua_value_integer_array(
    cpkt_opcua_value *value,
    const long *values,
    size_t value_count);
void cpkt_opcua_value_double_array(
    cpkt_opcua_value *value,
    const double *values,
    size_t value_count);
void cpkt_opcua_value_string_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_string_view *values,
    size_t value_count);
void cpkt_opcua_value_byte_string_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_byte_string_view *values,
    size_t value_count);
void cpkt_opcua_value_uint64_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_uint64 *values,
    size_t value_count);
void cpkt_opcua_value_datetime_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_datetime *values,
    size_t value_count);
void cpkt_opcua_value_status_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_status *values,
    size_t value_count);
void cpkt_opcua_value_guid_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_guid *values,
    size_t value_count);
void cpkt_opcua_value_qualified_name_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_qualified_name_view *values,
    size_t value_count);
void cpkt_opcua_value_localized_text_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_localized_text_view *values,
    size_t value_count);
void cpkt_opcua_data_value_clear(cpkt_opcua_data_value *data_value);
/* Initialize browse options to the same behavior as browse_children. */
void cpkt_opcua_browse_options_default(cpkt_opcua_browse_options *options);
/* Initialize data-change monitor options to upstream defaults. */
void cpkt_opcua_monitor_options_default(cpkt_opcua_monitor_options *options);
/* Initialize MQTT PubSub connection options to broker port 1883 and enabled. */
void cpkt_opcua_mqtt_connection_options_default(cpkt_opcua_mqtt_connection_options *options);
/* Initialize writer-group options to UADP encoding and enabled. */
void cpkt_opcua_pubsub_writer_group_options_default(
    cpkt_opcua_pubsub_writer_group_options *options);
/* Initialize data-set-writer options to enabled. */
void cpkt_opcua_pubsub_data_set_writer_options_default(
    cpkt_opcua_pubsub_data_set_writer_options *options);
/* Initialize reader-group options to UADP encoding and enabled. */
void cpkt_opcua_pubsub_reader_group_options_default(
    cpkt_opcua_pubsub_reader_group_options *options);
/* Initialize data-set-reader options to enabled. */
void cpkt_opcua_pubsub_data_set_reader_options_default(
    cpkt_opcua_pubsub_data_set_reader_options *options);

/*
 * Create a server bound to the requested port on startup. Use zero only when
 * the endpoint URL is not needed from the facade.
 */
cpkt_opcua_result cpkt_opcua_server_new(
    cpkt_opcua_server **out,
    unsigned short port);
/*
 * Create a server from explicit JSON5 configuration bytes. The bytes are
 * borrowed for the duration of the call. Endpoint URL helpers know the port
 * only after cpkt_opcua_server_set_endpoint is called.
 */
cpkt_opcua_result cpkt_opcua_server_new_from_json(
    cpkt_opcua_server **out,
    const unsigned char *json,
    size_t json_length,
    cpkt_opcua_status *status_out);
/*
 * Create a server from an explicit JSON5 configuration file path. The facade
 * reads only the named path; there is no implicit config-file discovery.
 */
cpkt_opcua_result cpkt_opcua_server_new_from_json_file(
    cpkt_opcua_server **out,
    const char *path,
    cpkt_opcua_status *status_out);
/* Free a server and shut it down first when needed. Accepts NULL. */
void cpkt_opcua_server_free(cpkt_opcua_server *server);
/* Start, iterate, and stop the server event loop. */
cpkt_opcua_result cpkt_opcua_server_startup(
    cpkt_opcua_server *server,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_iterate(
    cpkt_opcua_server *server,
    int wait_internal,
    unsigned short *wait_ms_out);
cpkt_opcua_result cpkt_opcua_server_shutdown(
    cpkt_opcua_server *server,
    cpkt_opcua_status *status_out);
/* Write the loopback endpoint URL into caller storage. */
cpkt_opcua_result cpkt_opcua_server_endpoint_url(
    const cpkt_opcua_server *server,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out);
cpkt_opcua_result cpkt_opcua_server_set_endpoint(
    cpkt_opcua_server *server,
    const char *hostname,
    unsigned short port);
/* Set server application identity before startup. Strings are copied. */
cpkt_opcua_result cpkt_opcua_server_set_application_identity(
    cpkt_opcua_server *server,
    const char *application_uri,
    const char *product_uri,
    const char *application_name);
/* Run a native config callback before startup. The config pointer is borrowed. */
cpkt_opcua_result cpkt_opcua_server_native_config(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Configure file/json server setup through the borrowed upstream config. */
cpkt_opcua_result cpkt_opcua_server_file_config_native_config(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Configure custom server security plugins through the borrowed upstream config. */
cpkt_opcua_result cpkt_opcua_server_security_plugin_native_config(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_set_default_security(
    cpkt_opcua_server *server,
    int secure_only,
    const unsigned char *certificate,
    size_t certificate_length,
    const unsigned char *private_key,
    size_t private_key_length,
    const cpkt_opcua_byte_string_view *trust_list,
    size_t trust_list_count,
    const cpkt_opcua_byte_string_view *issuer_list,
    size_t issuer_list_count,
    const cpkt_opcua_byte_string_view *revocation_list,
    size_t revocation_list_count,
    cpkt_opcua_status *status_out);
/* Configure anonymous and optional username/password login before startup. */
cpkt_opcua_result cpkt_opcua_server_set_access_control(
    cpkt_opcua_server *server,
    int allow_anonymous,
    const char *username,
    const char *password,
    cpkt_opcua_status *status_out);
/* Configure username/password login through a C89 callback before startup. */
cpkt_opcua_result cpkt_opcua_server_set_access_control_callback(
    cpkt_opcua_server *server,
    int allow_anonymous,
    cpkt_opcua_login_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Add a writable scalar variable under the standard objects folder. */
cpkt_opcua_result cpkt_opcua_server_add_variable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Add an object node under the provided parent. */
cpkt_opcua_result cpkt_opcua_server_add_object(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    cpkt_opcua_status *status_out);
/* Add a writable scalar variable under the provided parent. */
cpkt_opcua_result cpkt_opcua_server_add_variable_under(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Add common non-instance node classes. Type nodes are added with HasSubtype; views with Organizes. */
cpkt_opcua_result cpkt_opcua_server_add_object_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_variable_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_reference_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const char *inverse_name,
    int is_abstract,
    int symmetric,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_data_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_view(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int contains_no_loops,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out);
/* Add a method with scalar inputs and one scalar output under the parent. */
cpkt_opcua_result cpkt_opcua_server_add_method(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const int *input_types,
    size_t input_count,
    int output_type,
    cpkt_opcua_method_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Add a method with scalar inputs and multiple scalar outputs. */
cpkt_opcua_result cpkt_opcua_server_add_method_many(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const int *input_types,
    size_t input_count,
    const int *output_types,
    size_t output_count,
    cpkt_opcua_method_many_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Register a namespace URI on the server and return its namespace index. */
cpkt_opcua_result cpkt_opcua_server_add_namespace(
    cpkt_opcua_server *server,
    const char *namespace_uri,
    unsigned short *namespace_index_out);
/* Delete a node; nonzero also deletes references targeting that node. */
cpkt_opcua_result cpkt_opcua_server_delete_node(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int delete_target_refs,
    cpkt_opcua_status *status_out);
/* Add or delete a reference between existing nodes. */
cpkt_opcua_result cpkt_opcua_server_add_reference(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_reference_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_delete_reference(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_delete_reference_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out);
/* Invoke fn once for each browsed child. Callback data is borrowed. */
cpkt_opcua_result cpkt_opcua_server_browse_children(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id parent_node_id,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_browse_children_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_browse_children_page(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_browse_next(
    cpkt_opcua_server *server,
    const unsigned char *continuation_point,
    size_t continuation_point_size,
    int release_continuation_point,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *next_continuation_point_buffer,
    size_t next_continuation_point_buffer_size,
    size_t *required_next_continuation_point_size_out,
    cpkt_opcua_status *status_out);
/* Translate a forward hierarchical browse path and return the first target. */
cpkt_opcua_result cpkt_opcua_server_translate_browse_path(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements,
    size_t element_count,
    cpkt_opcua_node_id *target_node_id_out,
    char *target_buffer,
    size_t target_buffer_size,
    size_t *required_target_size_out,
    cpkt_opcua_status *status_out);
/* Read or write a scalar variable. String reads require caller storage. */
cpkt_opcua_result cpkt_opcua_server_read(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_value *value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
/* Read Value as a borrowed native payload for unsupported value types. */
cpkt_opcua_result cpkt_opcua_server_read_native_variant(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_variant_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Read Value as a borrowed native data value, including native status. */
cpkt_opcua_result cpkt_opcua_server_read_native_data_value(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_data_value_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_data_value(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_data_value *data_value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_boolean_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_integer_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_double_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_string_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_byte_string_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_uint64_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_datetime_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_status_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_guid_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_qualified_name_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_localized_text_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
/* Read or write an OPC UA NumericRange such as "1:3" against the Value attribute. */
cpkt_opcua_result cpkt_opcua_server_read_boolean_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_integer_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_double_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_string_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_byte_string_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_uint64_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_datetime_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_status_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_guid_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_qualified_name_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_localized_text_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_index_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Read common node attributes. String outputs require caller storage. */
cpkt_opcua_result cpkt_opcua_server_read_node_id(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *node_id_out,
    char *identifier_buffer,
    size_t identifier_buffer_size,
    size_t *required_identifier_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_node_class(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *node_class_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_browse_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned short *namespace_index_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_display_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_description(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_display_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *display_name,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_description(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *description,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_write_mask(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_user_write_mask(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_write_mask(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long write_mask,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_is_abstract(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *is_abstract_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_is_abstract(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_symmetric(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *symmetric_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_symmetric(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int symmetric,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_inverse_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_inverse_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *inverse_name,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_contains_no_loops(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *contains_no_loops_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_contains_no_loops(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int contains_no_loops,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_event_notifier(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *event_notifier_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_event_notifier(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_data_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *data_type_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_data_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id data_type,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_value_rank(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    long *value_rank_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_value_rank(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    long value_rank,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_array_dimensions(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *dimensions,
    size_t dimension_count,
    size_t *required_dimension_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_array_dimensions(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const unsigned long *dimensions,
    size_t dimension_count,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_access_level(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_user_access_level(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_access_level(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_access_level_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_access_level_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_minimum_sampling_interval(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    double *minimum_sampling_interval_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_minimum_sampling_interval(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    double minimum_sampling_interval,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_historizing(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *historizing_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_historizing(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int historizing,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_executable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_user_executable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_executable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int executable,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_method_argument_count(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t *argument_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_read_method_argument(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t argument_index,
    cpkt_opcua_node_id *data_type_out,
    long *value_rank_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *required_name_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/*
 * Create a prepared server event. The returned event owns copies of borrowed
 * input strings and must be released with cpkt_opcua_server_event_free().
 * Set fields before triggering to override or add event payload values.
 */
cpkt_opcua_result cpkt_opcua_server_create_event(
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id event_type_id,
    unsigned long severity,
    const char *message,
    cpkt_opcua_server_event **event_out,
    cpkt_opcua_status *status_out);
/* Free a prepared event. Free accepts NULL. */
void cpkt_opcua_server_event_free(cpkt_opcua_server_event *event);
/* Set or replace an event field by browse path, such as "Message". */
cpkt_opcua_result cpkt_opcua_server_event_set_field(
    cpkt_opcua_server_event *event,
    unsigned short namespace_index,
    const char *field_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Trigger a prepared event and copy the emitted EventId into caller storage. */
cpkt_opcua_result cpkt_opcua_server_event_trigger(
    cpkt_opcua_server *server,
    cpkt_opcua_server_event *event,
    unsigned char *event_id_buffer,
    size_t event_id_buffer_size,
    size_t *required_event_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_trigger_event(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id event_type_id,
    unsigned long severity,
    const char *message,
    unsigned char *event_id_buffer,
    size_t event_id_buffer_size,
    size_t *required_event_id_size_out,
    cpkt_opcua_status *status_out);
/* Run a native callback with the borrowed upstream server pointer. */
cpkt_opcua_result cpkt_opcua_server_native(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_fn fn,
    void *user);
/* Run native PubSub/MQTT configuration with the borrowed upstream server. */
cpkt_opcua_result cpkt_opcua_server_pubsub_native(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_fn fn,
    void *user);
/* Add a PubSub MQTT connection using common broker/topic parameters. */
cpkt_opcua_result cpkt_opcua_server_add_mqtt_pubsub_connection(
    cpkt_opcua_server *server,
    const cpkt_opcua_mqtt_connection_options *options,
    cpkt_opcua_node_id *connection_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
/* Add a published data set for published item fields. */
cpkt_opcua_result cpkt_opcua_server_add_published_dataset(
    cpkt_opcua_server *server,
    const char *name,
    cpkt_opcua_node_id *published_dataset_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
/* Add a published Value-attribute variable field to a published data set. */
cpkt_opcua_result cpkt_opcua_server_add_published_variable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id published_dataset_id,
    cpkt_opcua_node_id variable_node_id,
    const char *field_name,
    cpkt_opcua_node_id *field_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_pubsub_writer_group(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id connection_id,
    const cpkt_opcua_pubsub_writer_group_options *options,
    cpkt_opcua_node_id *writer_group_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_pubsub_data_set_writer(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id writer_group_id,
    cpkt_opcua_node_id published_dataset_id,
    const cpkt_opcua_pubsub_data_set_writer_options *options,
    cpkt_opcua_node_id *data_set_writer_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_pubsub_reader_group(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id connection_id,
    const cpkt_opcua_pubsub_reader_group_options *options,
    cpkt_opcua_node_id *reader_group_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_add_pubsub_data_set_reader(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id reader_group_id,
    const cpkt_opcua_pubsub_data_set_reader_options *options,
    cpkt_opcua_node_id *data_set_reader_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_write_pubsub_configuration(
    cpkt_opcua_server *server,
    unsigned char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_server_load_pubsub_configuration(
    cpkt_opcua_server *server,
    const unsigned char *buffer,
    size_t buffer_size,
    cpkt_opcua_status *status_out);
/* Run native history backend setup with the borrowed upstream server. */
cpkt_opcua_result cpkt_opcua_server_history_native(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_fn fn,
    void *user);

/* Create and free a client. Free accepts NULL. */
cpkt_opcua_result cpkt_opcua_client_new(cpkt_opcua_client **out);
void cpkt_opcua_client_free(cpkt_opcua_client *client);
/* Connect anonymously or with username/password credentials. */
cpkt_opcua_result cpkt_opcua_client_connect(
    cpkt_opcua_client *client,
    const char *endpoint_url,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_connect_username(
    cpkt_opcua_client *client,
    const char *endpoint_url,
    const char *username,
    const char *password,
    cpkt_opcua_status *status_out);
/* Run a native config callback before connecting. The config pointer is borrowed. */
cpkt_opcua_result cpkt_opcua_client_native_config(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Configure custom client security plugins through the borrowed upstream config. */
cpkt_opcua_result cpkt_opcua_client_security_plugin_native_config(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_set_default_encryption(
    cpkt_opcua_client *client,
    const unsigned char *certificate,
    size_t certificate_length,
    const unsigned char *private_key,
    size_t private_key_length,
    const cpkt_opcua_byte_string_view *trust_list,
    size_t trust_list_count,
    const cpkt_opcua_byte_string_view *revocation_list,
    size_t revocation_list_count,
    cpkt_opcua_status *status_out);
/* Disconnect and drive the client event loop. */
cpkt_opcua_result cpkt_opcua_client_disconnect(
    cpkt_opcua_client *client,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_run_iterate(
    cpkt_opcua_client *client,
    unsigned long timeout_ms,
    cpkt_opcua_status *status_out);
/* Resolve namespace URIs through the connected server. */
cpkt_opcua_result cpkt_opcua_client_get_namespace_index(
    cpkt_opcua_client *client,
    const char *namespace_uri,
    unsigned short *namespace_index_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_get_namespace_uri(
    cpkt_opcua_client *client,
    unsigned short namespace_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
/* Discover endpoint URLs exposed by a server URL. */
cpkt_opcua_result cpkt_opcua_client_get_endpoint_count(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t *endpoint_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_get_endpoint_url(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t endpoint_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
/* Discover registered server application descriptions. */
cpkt_opcua_result cpkt_opcua_client_find_server_count(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t *server_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_find_server_application_uri(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t server_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_find_server_application_name(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t server_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
/* Read or write a scalar value from a connected server. */
cpkt_opcua_result cpkt_opcua_client_read(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_value *value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
/* Read Value as a borrowed native payload for unsupported value types. */
cpkt_opcua_result cpkt_opcua_client_read_native_variant(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_variant_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
/* Read Value as a borrowed native data value, including native status. */
cpkt_opcua_result cpkt_opcua_client_read_native_data_value(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_data_value_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_data_value(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_data_value *data_value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
/* Read raw historical values for a node and deliver decoded DataValues. */
cpkt_opcua_result cpkt_opcua_client_history_read_raw(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime start_time,
    cpkt_opcua_datetime end_time,
    const char *index_range,
    int return_bounds,
    unsigned long values_per_response,
    cpkt_opcua_history_data_value_fn fn,
    void *user,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_boolean_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_integer_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_double_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_string_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_byte_string_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_uint64_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_datetime_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_status_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_guid_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_qualified_name_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_localized_text_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
/* Read or write an OPC UA NumericRange such as "1:3" against the Value attribute. */
cpkt_opcua_result cpkt_opcua_client_read_boolean_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_integer_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_double_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_string_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_byte_string_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_uint64_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_datetime_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_status_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_guid_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_qualified_name_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_localized_text_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_index_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Read common node attributes. String outputs require caller storage. */
cpkt_opcua_result cpkt_opcua_client_read_node_id(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *node_id_out,
    char *identifier_buffer,
    size_t identifier_buffer_size,
    size_t *required_identifier_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_node_class(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *node_class_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_browse_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned short *namespace_index_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_display_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_description(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_display_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *display_name,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_description(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *description,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_write_mask(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_user_write_mask(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_write_mask(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long write_mask,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_is_abstract(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *is_abstract_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_is_abstract(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_symmetric(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *symmetric_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_symmetric(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int symmetric,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_inverse_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_inverse_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *inverse_name,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_contains_no_loops(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *contains_no_loops_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_contains_no_loops(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int contains_no_loops,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_event_notifier(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *event_notifier_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_event_notifier(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_data_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *data_type_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_data_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id data_type,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_value_rank(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    long *value_rank_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_value_rank(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    long value_rank,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_array_dimensions(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *dimensions,
    size_t dimension_count,
    size_t *required_dimension_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_array_dimensions(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const unsigned long *dimensions,
    size_t dimension_count,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_access_level(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_user_access_level(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_access_level(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_access_level_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_access_level_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_minimum_sampling_interval(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    double *minimum_sampling_interval_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_minimum_sampling_interval(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    double minimum_sampling_interval,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_historizing(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *historizing_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_historizing(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int historizing,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_executable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_user_executable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_executable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int executable,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_method_argument_count(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t *argument_count_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_read_method_argument(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t argument_index,
    cpkt_opcua_node_id *data_type_out,
    long *value_rank_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *required_name_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Add an object node through the connected client. */
cpkt_opcua_result cpkt_opcua_client_add_object(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    cpkt_opcua_status *status_out);
/* Add a writable scalar variable under the standard objects folder. */
cpkt_opcua_result cpkt_opcua_client_add_variable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Add a writable scalar variable under the provided parent. */
cpkt_opcua_result cpkt_opcua_client_add_variable_under(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);
/* Add common non-instance node classes. Type nodes are added with HasSubtype; views with Organizes. */
cpkt_opcua_result cpkt_opcua_client_add_object_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_variable_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_reference_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const char *inverse_name,
    int is_abstract,
    int symmetric,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_data_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_view(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int contains_no_loops,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out);
/* Delete a node through the connected client. */
cpkt_opcua_result cpkt_opcua_client_delete_node(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int delete_target_refs,
    cpkt_opcua_status *status_out);
/* Add or delete a reference between existing nodes through the connected client. */
cpkt_opcua_result cpkt_opcua_client_add_reference(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_reference_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_delete_reference(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_delete_reference_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out);
/* Browse children through the connected client. Callback data is borrowed. */
cpkt_opcua_result cpkt_opcua_client_browse_children(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_browse_children_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_browse_children_page(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_browse_next(
    cpkt_opcua_client *client,
    const unsigned char *continuation_point,
    size_t continuation_point_size,
    int release_continuation_point,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *next_continuation_point_buffer,
    size_t next_continuation_point_buffer_size,
    size_t *required_next_continuation_point_size_out,
    cpkt_opcua_status *status_out);
/* Translate a forward hierarchical browse path and return the first target. */
cpkt_opcua_result cpkt_opcua_client_translate_browse_path(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements,
    size_t element_count,
    cpkt_opcua_node_id *target_node_id_out,
    char *target_buffer,
    size_t target_buffer_size,
    size_t *required_target_size_out,
    cpkt_opcua_status *status_out);
/* Call a method and decode its single scalar output. */
cpkt_opcua_result cpkt_opcua_client_call_method(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id,
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *output,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_call_method_many(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id,
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *outputs,
    size_t output_count,
    char **string_buffers,
    const size_t *string_buffer_sizes,
    size_t *required_string_sizes_out,
    cpkt_opcua_status *status_out);
/*
 * Submit selected asynchronous client operations. Output buffers and callback
 * userdata must stay valid until the callback runs or the client is freed.
 */
cpkt_opcua_result cpkt_opcua_client_read_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_async_value_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_write_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_async_status_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_browse_children_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn browse_fn,
    cpkt_opcua_async_browse_fn done_fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_call_method_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id,
    const cpkt_opcua_value *inputs,
    size_t input_count,
    size_t expected_output_count,
    cpkt_opcua_async_call_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_value *outputs,
    char **string_buffers,
    const size_t *string_buffer_sizes,
    size_t *required_string_sizes_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_object_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    cpkt_opcua_async_node_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_node_id *node_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_add_variable_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_async_node_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_node_id *node_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out);
/* Create and delete a client subscription. */
cpkt_opcua_result cpkt_opcua_client_create_subscription(
    cpkt_opcua_client *client,
    double publishing_interval_ms,
    cpkt_opcua_subscription_id *subscription_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_modify_subscription(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    double publishing_interval_ms,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_delete_subscription(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_status *status_out);
/* Monitor scalar value changes. The callback remains registered until delete. */
cpkt_opcua_result cpkt_opcua_client_monitor_value(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    double sampling_interval_ms,
    cpkt_opcua_data_change_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_monitor_value_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    const cpkt_opcua_monitor_options *options,
    cpkt_opcua_data_change_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_monitor_events(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    double sampling_interval_ms,
    cpkt_opcua_event_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_monitor_event_fields(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    double sampling_interval_ms,
    const cpkt_opcua_string_view *field_names,
    size_t field_count,
    cpkt_opcua_event_fields_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out);
cpkt_opcua_result cpkt_opcua_client_set_monitoring_mode(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    int monitoring_mode,
    cpkt_opcua_status *status_out);
/* Delete a monitored item owned by a subscription. */
cpkt_opcua_result cpkt_opcua_client_delete_monitored_item(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    cpkt_opcua_status *status_out);
/* Run a native callback with the borrowed upstream client pointer. */
cpkt_opcua_result cpkt_opcua_client_native(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_fn fn,
    void *user);
/* Run native async-service setup with the borrowed upstream client. */
cpkt_opcua_result cpkt_opcua_client_async_native(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_fn fn,
    void *user);
/* Run native history-service setup with the borrowed upstream client. */
cpkt_opcua_result cpkt_opcua_client_history_native(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_fn fn,
    void *user);

#ifdef __cplusplus
}
#endif

#endif
