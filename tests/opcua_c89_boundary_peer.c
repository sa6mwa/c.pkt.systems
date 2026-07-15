#include "opcua_c89_boundary_peer.h"

#include <cpkt/opcua.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CPKT_C89_FAIL(line) (line)
#define CPKT_C89_CHECK(expr)                                                   \
  do {                                                                         \
    if (!(expr)) {                                                             \
      return CPKT_C89_FAIL(__LINE__);                                          \
    }                                                                          \
  } while (0)

struct cpkt_opcua_c89_server_peer {
  cpkt_opcua_server *server;
};

struct cpkt_c89_browse_seen {
  int object_seen;
  int child_seen;
  int int_seen;
  int client_added_seen;
  int client_added_variable_seen;
  int client_added_child_variable_seen;
};

struct cpkt_c89_subscription_seen {
  int saw_expected;
  int saw_expected_bytes;
  int saw_expected_qualified_name;
  int saw_expected_localized_text;
  long last_integer;
  cpkt_opcua_status last_status;
};

struct cpkt_c89_event_seen {
  int saw_expected;
  int saw_expected_fields;
  unsigned long severity;
  long field_severity;
  size_t event_id_length;
  char message[64];
  char field_message[64];
};

struct cpkt_c89_string_array_seen {
  size_t count;
  int matched;
};

struct cpkt_c89_byte_string_array_seen {
  size_t count;
  int matched;
};

struct cpkt_c89_qualified_name_array_seen {
  size_t count;
  int matched;
};

struct cpkt_c89_localized_text_array_seen {
  size_t count;
  int matched;
};

struct cpkt_c89_async_seen {
  int done;
  int browse_seen;
  cpkt_opcua_result result;
  cpkt_opcua_status status;
  long integer_value;
  double double_value;
  int node_seen;
  unsigned long node_numeric;
};

static const unsigned char cpkt_c89_native_guid_node_id[16] = {
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
static const unsigned char cpkt_c89_native_byte_node_id[3] = {0xde, 0xad, 0xbe};
static const unsigned char cpkt_c89_facade_guid_node_id[16] = {
    0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21,
    0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21};
static const unsigned char cpkt_c89_facade_byte_node_id[4] = {0xca, 0xfe, 0xba,
                                                              0xbe};
static const unsigned char cpkt_c89_native_bytes_value[4] = {0x01, 0x23, 0x45,
                                                             0x67};
static const unsigned char cpkt_c89_native_bytes_updated[3] = {0x10, 0x20,
                                                               0x30};
static const unsigned char cpkt_c89_facade_bytes_value[5] = {0x89, 0xab, 0xcd,
                                                             0xef, 0x01};

static void cpkt_c89_async_value_callback(cpkt_opcua_request_id request_id,
                                          cpkt_opcua_result result,
                                          const cpkt_opcua_value *value,
                                          cpkt_opcua_status status,
                                          void *user) {
  struct cpkt_c89_async_seen *seen;

  (void)request_id;
  seen = (struct cpkt_c89_async_seen *)user;
  seen->done = 1;
  seen->result = result;
  seen->status = status;
  if (result == CPKT_OPCUA_OK && value != NULL &&
      value->type == CPKT_OPCUA_VALUE_INTEGER) {
    seen->integer_value = value->integer_value;
  }
}

static void cpkt_c89_async_status_callback(cpkt_opcua_request_id request_id,
                                           cpkt_opcua_result result,
                                           cpkt_opcua_status status,
                                           void *user) {
  struct cpkt_c89_async_seen *seen;

  (void)request_id;
  seen = (struct cpkt_c89_async_seen *)user;
  seen->done = 1;
  seen->result = result;
  seen->status = status;
}

static void
cpkt_c89_async_browse_done_callback(cpkt_opcua_request_id request_id,
                                    cpkt_opcua_result result,
                                    cpkt_opcua_status status, void *user) {
  struct cpkt_c89_async_seen *seen;

  (void)request_id;
  seen = (struct cpkt_c89_async_seen *)user;
  seen->done = 1;
  seen->result = result;
  seen->status = status;
}

static int
cpkt_c89_async_browse_entry_callback(const cpkt_opcua_browse_entry *entry,
                                     void *user) {
  struct cpkt_c89_async_seen *seen;

  seen = (struct cpkt_c89_async_seen *)user;
  if (entry != NULL && entry->browse_name != NULL &&
      (strcmp(entry->browse_name, "nativeAsyncInteger") == 0 ||
       strcmp(entry->browse_name, "facadeObject") == 0)) {
    seen->browse_seen = 1;
  }
  return 0;
}

static void cpkt_c89_async_call_callback(cpkt_opcua_request_id request_id,
                                         cpkt_opcua_result result,
                                         const cpkt_opcua_value *outputs,
                                         size_t output_count,
                                         cpkt_opcua_status status, void *user) {
  struct cpkt_c89_async_seen *seen;

  (void)request_id;
  seen = (struct cpkt_c89_async_seen *)user;
  seen->done = 1;
  seen->result = result;
  seen->status = status;
  if (result == CPKT_OPCUA_OK && outputs != NULL && output_count == 2) {
    if (outputs[0].type == CPKT_OPCUA_VALUE_INTEGER) {
      seen->integer_value = outputs[0].integer_value;
    }
    if (outputs[1].type == CPKT_OPCUA_VALUE_DOUBLE) {
      seen->double_value = outputs[1].double_value;
    }
  }
}

static void cpkt_c89_async_node_callback(cpkt_opcua_request_id request_id,
                                         cpkt_opcua_result result,
                                         const cpkt_opcua_node_id *node_id,
                                         cpkt_opcua_status status, void *user) {
  struct cpkt_c89_async_seen *seen;

  (void)request_id;
  seen = (struct cpkt_c89_async_seen *)user;
  seen->done = 1;
  seen->result = result;
  seen->status = status;
  if (result == CPKT_OPCUA_OK && node_id != NULL &&
      node_id->identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    seen->node_seen = 1;
    seen->node_numeric = node_id->numeric;
  }
}

static void cpkt_c89_wait_for_async(cpkt_opcua_client *client,
                                    struct cpkt_c89_async_seen *seen) {
  cpkt_opcua_status status;
  int attempt;

  for (attempt = 0; attempt < 100 && !seen->done; ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
}

static int cpkt_c89_browse_callback(const cpkt_opcua_browse_entry *entry,
                                    void *user) {
  struct cpkt_c89_browse_seen *seen;

  if (entry == NULL || user == NULL) {
    return 1;
  }
  seen = (struct cpkt_c89_browse_seen *)user;
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "nativeObject") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_NATIVE_OBJECT_ID) {
      return 1;
    }
    seen->object_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "nativeObjectChild") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_NATIVE_CHILD_ID) {
      return 1;
    }
    seen->child_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "nativeInteger") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_NATIVE_INT_ID) {
      return 1;
    }
    seen->int_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "nativeAsyncInteger") == 0) {
    seen->int_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "clientAddedObject") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_CLIENT_ADDED_OBJECT_ID) {
      return 1;
    }
    seen->client_added_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "clientAddedVariable") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_CLIENT_ADDED_VARIABLE_ID) {
      return 1;
    }
    seen->client_added_variable_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "clientAddedChildVariable") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric !=
            CPKT_OPCUA_CLIENT_ADDED_CHILD_VARIABLE_ID) {
      return 1;
    }
    seen->client_added_child_variable_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "facadeObject") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_FACADE_OBJECT_ID) {
      return 1;
    }
    seen->object_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "facadeObjectChild") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_FACADE_CHILD_ID) {
      return 1;
    }
    seen->child_seen = 1;
  }
  if (entry->browse_name != NULL &&
      strcmp(entry->browse_name, "facadeInteger") == 0) {
    if (entry->target_node_id.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC ||
        entry->target_node_id.numeric != CPKT_OPCUA_FACADE_INT_ID) {
      return 1;
    }
    seen->int_seen = 1;
  }
  return 0;
}

static cpkt_opcua_status cpkt_c89_native_client_seen(void *native_client,
                                                     void *user) {
  if (native_client == NULL || user == NULL) {
    return 1;
  }
  *(int *)user = 1;
  return 0;
}

static cpkt_opcua_status cpkt_c89_native_server_seen(void *native_server,
                                                     void *user) {
  if (native_server == NULL || user == NULL) {
    return 1;
  }
  *(int *)user = 1;
  return 0;
}

static cpkt_opcua_status cpkt_c89_native_config_seen(void *native_config,
                                                     void *user) {
  if (native_config == NULL || user == NULL) {
    return 1;
  }
  *(int *)user = 1;
  return 0;
}

struct cpkt_c89_history_seen {
  unsigned long count;
  long previous_value;
  long last_value;
};

static int
cpkt_c89_history_seen_callback(const cpkt_opcua_data_value *data_value,
                               int more_data_available, void *user) {
  struct cpkt_c89_history_seen *seen;

  (void)more_data_available;
  if (data_value == NULL || user == NULL) {
    return 1;
  }
  if (data_value->has_value == 0 ||
      data_value->value.type != CPKT_OPCUA_VALUE_INTEGER) {
    return 1;
  }
  seen = (struct cpkt_c89_history_seen *)user;
  seen->previous_value = seen->last_value;
  seen->last_value = data_value->value.integer_value;
  ++seen->count;
  return 0;
}

static int cpkt_c89_native_variant_seen(const void *native_variant,
                                        void *user) {
  if (native_variant == NULL || user == NULL) {
    return 1;
  }
  *(int *)user = 1;
  return 0;
}

static int cpkt_c89_native_data_value_seen(const void *native_data_value,
                                           void *user) {
  if (native_data_value == NULL || user == NULL) {
    return 1;
  }
  *(int *)user = 1;
  return 0;
}

static void
cpkt_c89_subscription_callback(cpkt_opcua_subscription_id subscription_id,
                               cpkt_opcua_monitored_item_id monitored_item_id,
                               const cpkt_opcua_value *value,
                               cpkt_opcua_status status, void *user) {
  struct cpkt_c89_subscription_seen *seen;

  if (subscription_id == 0 || monitored_item_id == 0 || value == NULL ||
      user == NULL) {
    return;
  }
  seen = (struct cpkt_c89_subscription_seen *)user;
  seen->last_status = status;
  if (status == 0 && value->type == CPKT_OPCUA_VALUE_INTEGER) {
    seen->last_integer = value->integer_value;
    if (value->integer_value == 43 || value->integer_value == 44) {
      seen->saw_expected = 1;
    }
  } else if (status == 0 && value->type == CPKT_OPCUA_VALUE_BYTE_STRING) {
    if (value->bytes_length == sizeof(cpkt_c89_native_bytes_updated) &&
        memcmp(value->bytes_value, cpkt_c89_native_bytes_updated,
               value->bytes_length) == 0) {
      seen->saw_expected_bytes = 1;
    }
  } else if (status == 0 && value->type == CPKT_OPCUA_VALUE_QUALIFIED_NAME) {
    if (value->qualified_name_namespace_index == CPKT_OPCUA_TEST_NS &&
        value->qualified_name_length == strlen("monitorQualified") &&
        memcmp(value->qualified_name, "monitorQualified",
               value->qualified_name_length) == 0) {
      seen->saw_expected_qualified_name = 1;
    }
  } else if (status == 0 && value->type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT) {
    if (value->localized_text_locale_length == strlen("en-US") &&
        memcmp(value->localized_text_locale, "en-US",
               value->localized_text_locale_length) == 0 &&
        value->localized_text_length == strlen("monitor text") &&
        memcmp(value->localized_text, "monitor text",
               value->localized_text_length) == 0) {
      seen->saw_expected_localized_text = 1;
    }
  }
}

static void
cpkt_c89_event_callback(cpkt_opcua_subscription_id subscription_id,
                        cpkt_opcua_monitored_item_id monitored_item_id,
                        const cpkt_opcua_event *event, cpkt_opcua_status status,
                        void *user) {
  struct cpkt_c89_event_seen *seen;
  size_t copy_length;

  if (subscription_id == 0 || monitored_item_id == 0 || event == NULL ||
      user == NULL || status != 0) {
    return;
  }
  seen = (struct cpkt_c89_event_seen *)user;
  seen->severity = event->severity;
  seen->event_id_length = event->event_id_length;
  copy_length = event->message_length;
  if (copy_length >= sizeof(seen->message)) {
    copy_length = sizeof(seen->message) - 1;
  }
  if (copy_length != 0 && event->message != NULL) {
    memcpy(seen->message, event->message, copy_length);
  }
  seen->message[copy_length] = '\0';
  if (((event->severity == 321UL &&
        strcmp(seen->message, "native method event") == 0) ||
       (event->severity == 654UL &&
        strcmp(seen->message, "facade server event") == 0)) &&
      event->event_id_length != 0) {
    seen->saw_expected = 1;
  }
}

static int cpkt_c89_event_field_matches(const cpkt_opcua_event_field *field,
                                        const char *name) {
  size_t name_length;
  size_t offset;

  if (field == NULL || field->name == NULL || name == NULL) {
    return 0;
  }
  name_length = strlen(name);
  if (field->name_length == name_length &&
      memcmp(field->name, name, name_length) == 0) {
    return 1;
  }
  if (field->name_length > name_length &&
      field->name[field->name_length - name_length - 1] == '/') {
    offset = field->name_length - name_length;
    return memcmp(field->name + offset, name, name_length) == 0;
  }
  return 0;
}

static void
cpkt_c89_event_fields_callback(cpkt_opcua_subscription_id subscription_id,
                               cpkt_opcua_monitored_item_id monitored_item_id,
                               const cpkt_opcua_event_field *fields,
                               size_t field_count, cpkt_opcua_status status,
                               void *user) {
  struct cpkt_c89_event_seen *seen;
  size_t i;
  size_t copy_length;
  int saw_message;
  int saw_severity;

  (void)status;
  if (subscription_id == 0 || monitored_item_id == 0 || fields == NULL ||
      user == NULL) {
    return;
  }
  seen = (struct cpkt_c89_event_seen *)user;
  saw_message = 0;
  saw_severity = 0;
  for (i = 0; i < field_count; ++i) {
    if (cpkt_c89_event_field_matches(&fields[i], "Message") &&
        fields[i].value.type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT) {
      copy_length = fields[i].value.localized_text_length;
      if (copy_length >= sizeof(seen->field_message)) {
        copy_length = sizeof(seen->field_message) - 1;
      }
      if (copy_length != 0 && fields[i].value.localized_text != NULL) {
        memcpy(seen->field_message, fields[i].value.localized_text,
               copy_length);
      }
      seen->field_message[copy_length] = '\0';
      saw_message = 1;
    } else if (cpkt_c89_event_field_matches(&fields[i], "Severity") &&
               fields[i].value.type == CPKT_OPCUA_VALUE_INTEGER) {
      seen->field_severity = fields[i].value.integer_value;
      saw_severity = 1;
    }
  }
  if (saw_message && saw_severity &&
      ((seen->field_severity == 321L &&
        strcmp(seen->field_message, "native method event") == 0) ||
       (seen->field_severity == 654L &&
        strcmp(seen->field_message, "facade server event") == 0))) {
    seen->saw_expected_fields = 1;
  }
}

static int cpkt_c89_string_array_callback(size_t index, const char *data,
                                          size_t length, void *user) {
  struct cpkt_c89_string_array_seen *seen;

  if (user == NULL || data == NULL) {
    return 1;
  }
  seen = (struct cpkt_c89_string_array_seen *)user;
  if ((index == 0 && length == strlen("alpha") &&
       memcmp(data, "alpha", length) == 0) ||
      (index == 1 && length == strlen("beta") &&
       memcmp(data, "beta", length) == 0)) {
    ++seen->matched;
  }
  ++seen->count;
  return 0;
}

static int cpkt_c89_byte_string_array_callback(size_t index,
                                               const unsigned char *data,
                                               size_t length, void *user) {
  struct cpkt_c89_byte_string_array_seen *seen;

  if (user == NULL || data == NULL) {
    return 1;
  }
  seen = (struct cpkt_c89_byte_string_array_seen *)user;
  if ((index == 0 && length == 2 && data[0] == 0x10U && data[1] == 0x11U) ||
      (index == 1 && length == 3 && data[0] == 0x20U && data[1] == 0x21U &&
       data[2] == 0x22U)) {
    ++seen->matched;
  }
  ++seen->count;
  return 0;
}

static int cpkt_c89_qualified_name_array_callback(
    size_t index, unsigned short namespace_index, const char *name,
    size_t name_length, void *user) {
  struct cpkt_c89_qualified_name_array_seen *seen;

  if (user == NULL || name == NULL) {
    return 1;
  }
  seen = (struct cpkt_c89_qualified_name_array_seen *)user;
  (void)index;
  if (namespace_index == CPKT_OPCUA_TEST_NS &&
      ((name_length == strlen("alphaName") &&
        memcmp(name, "alphaName", name_length) == 0) ||
       (name_length == strlen("betaName") &&
        memcmp(name, "betaName", name_length) == 0))) {
    ++seen->matched;
  }
  ++seen->count;
  return 0;
}

static int
cpkt_c89_localized_text_array_callback(size_t index, const char *locale,
                                       size_t locale_length, const char *text,
                                       size_t text_length, void *user) {
  struct cpkt_c89_localized_text_array_seen *seen;

  if (user == NULL || locale == NULL || text == NULL) {
    return 1;
  }
  seen = (struct cpkt_c89_localized_text_array_seen *)user;
  (void)index;
  if ((locale_length == strlen("en-US") &&
       memcmp(locale, "en-US", locale_length) == 0 &&
       text_length == strlen("alpha text") &&
       memcmp(text, "alpha text", text_length) == 0) ||
      (locale_length == strlen("sv-SE") &&
       memcmp(locale, "sv-SE", locale_length) == 0 &&
       text_length == strlen("beta text") &&
       memcmp(text, "beta text", text_length) == 0)) {
    ++seen->matched;
  }
  ++seen->count;
  return 0;
}

static cpkt_opcua_result
cpkt_c89_multiply_method(const cpkt_opcua_value *inputs, size_t input_count,
                         cpkt_opcua_value *output, void *user) {
  long factor;

  if (inputs == NULL || output == NULL || user == NULL || input_count != 1 ||
      inputs[0].type != CPKT_OPCUA_VALUE_INTEGER) {
    return CPKT_OPCUA_ERR_ARG;
  }
  factor = *(long *)user;
  cpkt_opcua_value_integer(output, inputs[0].integer_value * factor);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result
cpkt_c89_echo_string_method(const cpkt_opcua_value *inputs, size_t input_count,
                            cpkt_opcua_value *output, void *user) {
  (void)user;
  if (inputs == NULL || output == NULL || input_count != 1 ||
      inputs[0].type != CPKT_OPCUA_VALUE_STRING) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_string(output, inputs[0].string_value,
                          inputs[0].string_length);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result
cpkt_c89_echo_bytes_method(const cpkt_opcua_value *inputs, size_t input_count,
                           cpkt_opcua_value *output, void *user) {
  (void)user;
  if (inputs == NULL || output == NULL || input_count != 1 ||
      inputs[0].type != CPKT_OPCUA_VALUE_BYTE_STRING) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_byte_string(output, inputs[0].bytes_value,
                               inputs[0].bytes_length);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result
cpkt_c89_echo_qualified_name_method(const cpkt_opcua_value *inputs,
                                    size_t input_count,
                                    cpkt_opcua_value *output, void *user) {
  (void)user;
  if (inputs == NULL || output == NULL || input_count != 1 ||
      inputs[0].type != CPKT_OPCUA_VALUE_QUALIFIED_NAME) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_qualified_name(
      output, inputs[0].qualified_name_namespace_index,
      inputs[0].qualified_name, inputs[0].qualified_name_length);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result
cpkt_c89_echo_localized_text_method(const cpkt_opcua_value *inputs,
                                    size_t input_count,
                                    cpkt_opcua_value *output, void *user) {
  (void)user;
  if (inputs == NULL || output == NULL || input_count != 1 ||
      inputs[0].type != CPKT_OPCUA_VALUE_LOCALIZED_TEXT) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_localized_text(output, inputs[0].localized_text_locale,
                                  inputs[0].localized_text_locale_length,
                                  inputs[0].localized_text,
                                  inputs[0].localized_text_length);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_c89_empty_method(const cpkt_opcua_value *inputs,
                                               size_t input_count,
                                               cpkt_opcua_value *output,
                                               void *user) {
  (void)inputs;
  (void)user;
  if (input_count != 0 || output == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_clear(output);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_c89_multi_method(const cpkt_opcua_value *inputs,
                                               size_t input_count,
                                               cpkt_opcua_value *outputs,
                                               size_t output_count,
                                               void *user) {
  (void)user;
  if (inputs == NULL || outputs == NULL || input_count != 1 ||
      output_count != 2 || inputs[0].type != CPKT_OPCUA_VALUE_INTEGER) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_integer(&outputs[0], inputs[0].integer_value + 1);
  cpkt_opcua_value_double(&outputs[1], (double)inputs[0].integer_value + 0.25);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_status cpkt_c89_login_callback(const char *username,
                                                 size_t username_length,
                                                 const unsigned char *password,
                                                 size_t password_length,
                                                 void *user) {
  (void)user;
  if (username != NULL && password != NULL &&
      username_length == strlen("c89-user") &&
      password_length == strlen("c89-secret") &&
      memcmp(username, "c89-user", username_length) == 0 &&
      memcmp(password, "c89-secret", password_length) == 0) {
    return 0;
  }
  return 0x801f0000UL;
}

int cpkt_opcua_c89_client_exercise_native_server(const char *endpoint) {
  cpkt_opcua_client *client;
  cpkt_opcua_node_id int_node;
  cpkt_opcua_node_id async_int_node;
  cpkt_opcua_node_id history_value_node;
  cpkt_opcua_node_id bool_node;
  cpkt_opcua_node_id string_node;
  cpkt_opcua_node_id native_object_node;
  cpkt_opcua_node_id native_child_node;
  cpkt_opcua_node_id native_method_node;
  cpkt_opcua_node_id native_multi_method_node;
  cpkt_opcua_node_id native_reference_type_node;
  cpkt_opcua_node_id native_view_node;
  cpkt_opcua_node_id client_added_node;
  cpkt_opcua_node_id client_added_variable_node;
  cpkt_opcua_node_id client_added_child_variable_node;
  cpkt_opcua_node_id client_added_object_type_node;
  cpkt_opcua_node_id client_added_variable_type_node;
  cpkt_opcua_node_id client_added_array_variable_type_node;
  cpkt_opcua_node_id client_added_reference_type_node;
  cpkt_opcua_node_id client_added_data_type_node;
  cpkt_opcua_node_id client_added_view_node;
  cpkt_opcua_node_id native_guid_node;
  cpkt_opcua_node_id native_byte_node;
  cpkt_opcua_node_id bytes_node;
  cpkt_opcua_node_id array_node;
  cpkt_opcua_node_id bool_array_node;
  cpkt_opcua_node_id double_array_node;
  cpkt_opcua_node_id string_array_node;
  cpkt_opcua_node_id bytes_array_node;
  cpkt_opcua_node_id guid_value_node;
  cpkt_opcua_node_id guid_array_node;
  cpkt_opcua_node_id status_value_node;
  cpkt_opcua_node_id status_array_node;
  cpkt_opcua_node_id uint64_value_node;
  cpkt_opcua_node_id uint64_array_node;
  cpkt_opcua_node_id datetime_value_node;
  cpkt_opcua_node_id datetime_array_node;
  cpkt_opcua_node_id qualified_name_value_node;
  cpkt_opcua_node_id qualified_name_array_node;
  cpkt_opcua_node_id localized_text_value_node;
  cpkt_opcua_node_id localized_text_array_node;
  cpkt_opcua_node_id parsed_node;
  cpkt_opcua_node_id constructed_string_node;
  cpkt_opcua_node_id translated_node;
  cpkt_opcua_node_id argument_data_type;
  cpkt_opcua_browse_path_element object_child_path[2];
  cpkt_opcua_browse_path_element string_path[1];
  cpkt_opcua_browse_path_element missing_path[1];
  cpkt_opcua_string_view event_field_names[3];
  cpkt_opcua_monitor_options monitor_options;
  cpkt_opcua_value value;
  cpkt_opcua_value inputs[1];
  cpkt_opcua_value outputs[2];
  cpkt_opcua_value out;
  cpkt_opcua_data_value data_value;
  cpkt_opcua_status status;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  cpkt_opcua_monitored_item_id event_monitored_item_id;
  struct cpkt_c89_browse_seen browse_seen;
  struct cpkt_c89_subscription_seen subscription_seen;
  struct cpkt_c89_event_seen event_seen;
  cpkt_opcua_browse_options browse_options;
  char string_buffer[32];
  char namespace_buffer[64];
  char name_buffer[64];
  char node_id_buffer[64];
  char parsed_string_buffer[64];
  char endpoint_buffer[128];
  char server_buffer[128];
  unsigned char continuation_point[64];
  unsigned char next_continuation_point[64];
  unsigned char small_continuation_point[1];
  size_t required;
  size_t next_required;
  size_t endpoint_count;
  size_t server_count;
  size_t argument_count;
  size_t dimension_count;
  size_t array_count;
  unsigned short namespace_index;
  unsigned long node_class;
  unsigned long access_level;
  unsigned long write_mask;
  unsigned long original_write_mask;
  unsigned long event_notifier;
  unsigned long dimensions[2];
  cpkt_opcua_guid guid_array_values[2];
  cpkt_opcua_status status_array_values[2];
  int boolean_array_values[2];
  long integer_array_values[3];
  cpkt_opcua_uint64 uint64_array_values[2];
  cpkt_opcua_datetime datetime_array_values[3];
  double double_array_values[2];
  struct cpkt_c89_string_array_seen string_array_seen;
  struct cpkt_c89_byte_string_array_seen byte_string_array_seen;
  struct cpkt_c89_qualified_name_array_seen qualified_name_array_seen;
  struct cpkt_c89_localized_text_array_seen localized_text_array_seen;
  struct cpkt_c89_history_seen history_seen;
  struct cpkt_c89_async_seen async_seen;
  cpkt_opcua_datetime history_start;
  cpkt_opcua_datetime history_end;
  cpkt_opcua_request_id request_id;
  long value_rank;
  long argument_value_rank;
  double minimum_sampling_interval;
  int executable;
  int historizing;
  int boolean_attribute;
  int native_seen;
  int native_variant_seen;
  int native_data_value_seen;
  int lifecycle_iteration;
  int attempt;

  CPKT_C89_CHECK(endpoint != NULL);
  client = NULL;
  CPKT_C89_CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_connect(client, endpoint, &status) ==
                 CPKT_OPCUA_OK);

  CPKT_C89_CHECK(
      cpkt_opcua_client_get_endpoint_count(client, endpoint, &endpoint_count,
                                           &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(endpoint_count > 0);
  CPKT_C89_CHECK(cpkt_opcua_client_get_endpoint_url(
                     client, endpoint, 0, endpoint_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(
      cpkt_opcua_client_get_endpoint_url(client, endpoint, 0, endpoint_buffer,
                                         sizeof(endpoint_buffer), &required,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strncmp(endpoint_buffer, "opc.tcp://", strlen("opc.tcp://")) ==
                 0);
  CPKT_C89_CHECK(cpkt_opcua_client_get_endpoint_url(
                     client, endpoint, endpoint_count, endpoint_buffer,
                     sizeof(endpoint_buffer), &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_count(client, endpoint,
                                                     &server_count,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(server_count > 0);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_uri(
                     client, endpoint, 0, server_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_uri(
                     client, endpoint, 0, server_buffer, sizeof(server_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(required > 1);
  CPKT_C89_CHECK(server_buffer[0] != '\0');
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_name(
                     client, endpoint, 0, server_buffer, sizeof(server_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(required > 1);
  CPKT_C89_CHECK(server_buffer[0] != '\0');
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_name(
                     client, endpoint, server_count, server_buffer,
                     sizeof(server_buffer), &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);

  int_node =
      cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_INT_ID);
  async_int_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 7799);
  history_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID);
  bool_node =
      cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_BOOL_ID);
  string_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_NATIVE_STRING_ID);
  native_object_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                                  CPKT_OPCUA_NATIVE_OBJECT_ID);
  native_child_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                                 CPKT_OPCUA_NATIVE_CHILD_ID);
  native_method_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                                  CPKT_OPCUA_NATIVE_METHOD_ID);
  native_multi_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_MULTI_METHOD_ID);
  native_reference_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_REFERENCE_TYPE_ID);
  native_view_node =
      cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_VIEW_ID);
  client_added_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_OBJECT_ID);
  client_added_variable_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_VARIABLE_ID);
  client_added_child_variable_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_CHILD_VARIABLE_ID);
  client_added_object_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_OBJECT_TYPE_ID);
  client_added_variable_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_VARIABLE_TYPE_ID);
  client_added_array_variable_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_ARRAY_VARIABLE_TYPE_ID);
  client_added_reference_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_REFERENCE_TYPE_ID);
  client_added_data_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_DATA_TYPE_ID);
  client_added_view_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_CLIENT_ADDED_VIEW_ID);
  bytes_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_NATIVE_BYTES_ID);
  array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_NATIVE_ARRAY_ID);
  bool_array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                               CPKT_OPCUA_NATIVE_BOOL_ARRAY_ID);
  double_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_DOUBLE_ARRAY_ID);
  string_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STRING_ARRAY_ID);
  bytes_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_BYTES_ARRAY_ID);
  guid_value_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                               CPKT_OPCUA_NATIVE_GUID_VALUE_ID);
  guid_array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                               CPKT_OPCUA_NATIVE_GUID_ARRAY_ID);
  status_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STATUS_VALUE_ID);
  status_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STATUS_ARRAY_ID);
  uint64_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_UINT64_VALUE_ID);
  uint64_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_UINT64_ARRAY_ID);
  datetime_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_DATETIME_VALUE_ID);
  datetime_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_DATETIME_ARRAY_ID);
  qualified_name_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_QUALIFIED_NAME_VALUE_ID);
  qualified_name_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_QUALIFIED_NAME_ARRAY_ID);
  localized_text_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_LOCALIZED_TEXT_VALUE_ID);
  localized_text_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_LOCALIZED_TEXT_ARRAY_ID);
  native_guid_node =
      cpkt_opcua_node_id_guid(CPKT_OPCUA_TEST_NS, cpkt_c89_native_guid_node_id);
  native_byte_node = cpkt_opcua_node_id_byte_string(
      CPKT_OPCUA_TEST_NS, cpkt_c89_native_byte_node_id,
      sizeof(cpkt_c89_native_byte_node_id));
  constructed_string_node = cpkt_opcua_node_id_string(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STRING_NODE_ID);

  memset(&async_seen, 0, sizeof(async_seen));
  cpkt_opcua_value_integer(&value, 555);
  CPKT_C89_CHECK(cpkt_opcua_client_write_async(client, async_int_node, &value,
                                               cpkt_c89_async_status_callback,
                                               &async_seen, &request_id,
                                               &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.status == 0);

  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_read_async(client, async_int_node,
                                              cpkt_c89_async_value_callback,
                                              &async_seen, &request_id, NULL, 0,
                                              NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.integer_value == 555);

  cpkt_opcua_value_integer(&inputs[0], 6);
  memset(outputs, 0, sizeof(outputs));
  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_call_method_async(
                     client, native_object_node, native_multi_method_node,
                     inputs, 1, 2, cpkt_c89_async_call_callback, &async_seen,
                     &request_id, outputs, NULL, NULL, NULL,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.integer_value == 12);
  CPKT_C89_CHECK(async_seen.double_value > 6.4 &&
                 async_seen.double_value < 6.6);

  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_async(
                     client, cpkt_opcua_node_id_numeric(0, 85), NULL,
                     cpkt_c89_async_browse_entry_callback,
                     cpkt_c89_async_browse_done_callback, &async_seen,
                     &request_id, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.browse_seen == 1);

  native_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_native(client, cpkt_c89_native_client_seen,
                                          &native_seen) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  native_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_async_native(client,
                                                cpkt_c89_native_client_seen,
                                                &native_seen) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  native_seen = 0;
  CPKT_C89_CHECK(
      cpkt_opcua_client_history_native(client, cpkt_c89_native_client_seen,
                                       &native_seen) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  native_variant_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_read_native_variant(
                     client, int_node, cpkt_c89_native_variant_seen,
                     &native_variant_seen, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_variant_seen == 1);
  native_data_value_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_read_native_data_value(
                     client, int_node, cpkt_c89_native_data_value_seen,
                     &native_data_value_seen, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_data_value_seen == 1);
  history_seen.count = 0;
  history_seen.previous_value = 0;
  history_seen.last_value = 0;
  history_start.high32 = 0;
  history_start.low32 = 0;
  history_end.high32 = 0x7fffffffL;
  history_end.low32 = 0xffffffffUL;
  CPKT_C89_CHECK(cpkt_opcua_client_history_read_raw(
                     client, history_value_node, history_start, history_end,
                     NULL, 0, 16, cpkt_c89_history_seen_callback, &history_seen,
                     NULL, 0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(history_seen.count >= 2UL);
  CPKT_C89_CHECK(history_seen.previous_value == 111L);
  CPKT_C89_CHECK(history_seen.last_value == 222L);

  CPKT_C89_CHECK(cpkt_opcua_client_get_namespace_index(
                     client, CPKT_OPCUA_NATIVE_NAMESPACE_URI, &namespace_index,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(namespace_index != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_get_namespace_uri(
                     client, namespace_index, namespace_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(cpkt_opcua_client_get_namespace_uri(
                     client, namespace_index, namespace_buffer,
                     sizeof(namespace_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(namespace_buffer, CPKT_OPCUA_NATIVE_NAMESPACE_URI) ==
                 0);

  object_child_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  object_child_path[0].browse_name = "nativeObject";
  object_child_path[1].namespace_index = CPKT_OPCUA_TEST_NS;
  object_child_path[1].browse_name = "nativeObjectChild";
  CPKT_C89_CHECK(cpkt_opcua_client_translate_browse_path(
                     client, cpkt_opcua_node_id_numeric(0, 85),
                     object_child_path, 2, &translated_node, NULL, 0, &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_node_id_equal(translated_node, native_child_node) ==
                 1);
  string_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  string_path[0].browse_name = "nativeStringNode";
  CPKT_C89_CHECK(cpkt_opcua_client_translate_browse_path(
                     client, cpkt_opcua_node_id_numeric(0, 85), string_path, 1,
                     &translated_node, node_id_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen(CPKT_OPCUA_NATIVE_STRING_NODE_ID) + 1);
  CPKT_C89_CHECK(cpkt_opcua_client_translate_browse_path(
                     client, cpkt_opcua_node_id_numeric(0, 85), string_path, 1,
                     &translated_node, node_id_buffer, sizeof(node_id_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(
      cpkt_opcua_node_id_equal(translated_node, constructed_string_node) == 1);
  missing_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  missing_path[0].browse_name = "missingNativeChild";
  CPKT_C89_CHECK(cpkt_opcua_client_translate_browse_path(
                     client, cpkt_opcua_node_id_numeric(0, 85), missing_path, 1,
                     &translated_node, node_id_buffer, sizeof(node_id_buffer),
                     &required, &status) == CPKT_OPCUA_ERR_UPSTREAM);

  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(client, native_object_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_OBJECT);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_node_id(client, native_object_node, &parsed_node,
                                     node_id_buffer, sizeof(node_id_buffer),
                                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_node_id_equal(parsed_node, native_object_node) ==
                 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_id(client, constructed_string_node,
                                                &parsed_node, node_id_buffer, 4,
                                                &required, &status) ==
                 CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen(CPKT_OPCUA_NATIVE_STRING_NODE_ID) + 1);
  CPKT_C89_CHECK(parsed_node.identifier_type == CPKT_OPCUA_NODE_ID_NULL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_id(
                     client, constructed_string_node, &parsed_node,
                     node_id_buffer, sizeof(node_id_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(parsed_node.identifier_type == CPKT_OPCUA_NODE_ID_STRING);
  CPKT_C89_CHECK(parsed_node.namespace_index == CPKT_OPCUA_TEST_NS);
  CPKT_C89_CHECK(strcmp(parsed_node.string, CPKT_OPCUA_NATIVE_STRING_NODE_ID) ==
                 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(client, native_child_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VARIABLE);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(client, native_method_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_METHOD);
  CPKT_C89_CHECK(cpkt_opcua_client_read_data_type(
                     client, int_node, &parsed_node, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(parsed_node.namespace_index == 0);
  CPKT_C89_CHECK(parsed_node.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(parsed_node.numeric == CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(cpkt_opcua_client_read_value_rank(
                     client, int_node, &value_rank, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_access_level(client, int_node,
                                                     &access_level,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_write_mask(
                     client, int_node, &write_mask, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((write_mask & 1UL) != 0);
  original_write_mask = write_mask;
  CPKT_C89_CHECK(cpkt_opcua_client_read_user_write_mask(
                     client, int_node, &write_mask, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((write_mask & 1UL) != 0);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_write_mask(client, int_node, original_write_mask,
                                         &status) == CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_write_mask(
                     client, int_node, &write_mask, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(write_mask == original_write_mask);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_write_mask(client, int_node, 1UL, &status) ==
      CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_write_mask(
                     client, int_node, &write_mask, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(write_mask == original_write_mask);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_data_type(
          client, int_node,
          cpkt_opcua_node_id_numeric(0, CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER),
          &status) == CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_value_rank(
                     client, int_node, -1, &status) == CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_array_dimensions(
                     client, array_node, dimensions, 0, &dimension_count,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_array_dimensions(
                     client, array_node, dimensions, 1, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(dimensions[0] == 3UL);
  dimensions[0] = 4UL;
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_array_dimensions(client, array_node, dimensions,
                                               1, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_array_dimensions(
                     client, array_node, dimensions, 1, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(dimensions[0] == 4UL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_integer_array(
                     client, array_node, integer_array_values, 2, &array_count,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(array_count == 3);
  CPKT_C89_CHECK(cpkt_opcua_client_read_integer_array(
                     client, array_node, integer_array_values, 3, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 3);
  CPKT_C89_CHECK(integer_array_values[0] == 1);
  CPKT_C89_CHECK(integer_array_values[1] == 2);
  CPKT_C89_CHECK(integer_array_values[2] == 3);
  CPKT_C89_CHECK(cpkt_opcua_client_read_integer_array_range(
                     client, array_node, "1:2", integer_array_values, 2,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(integer_array_values[0] == 2);
  CPKT_C89_CHECK(integer_array_values[1] == 3);
  integer_array_values[0] = 20;
  integer_array_values[1] = 21;
  cpkt_opcua_value_integer_array(&value, integer_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_client_write_index_range(client, array_node, "1:2",
                                                     &value,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_integer_array(
                     client, array_node, integer_array_values, 3, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 3);
  CPKT_C89_CHECK(integer_array_values[0] == 1);
  CPKT_C89_CHECK(integer_array_values[1] == 20);
  CPKT_C89_CHECK(integer_array_values[2] == 21);
  CPKT_C89_CHECK(cpkt_opcua_client_read_boolean_array(
                     client, bool_array_node, boolean_array_values, 2,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(boolean_array_values[0] == 1);
  CPKT_C89_CHECK(boolean_array_values[1] == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_boolean_array_range(
                     client, bool_array_node, "1", boolean_array_values, 1,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(boolean_array_values[0] == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_double_array(
                     client, double_array_node, double_array_values, 2,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(double_array_values[0] > 1.24 &&
                 double_array_values[0] < 1.26);
  CPKT_C89_CHECK(double_array_values[1] > 2.49 &&
                 double_array_values[1] < 2.51);
  CPKT_C89_CHECK(cpkt_opcua_client_read_double_array_range(
                     client, double_array_node, "1", double_array_values, 1,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(double_array_values[0] > 2.49 &&
                 double_array_values[0] < 2.51);
  string_array_seen.count = 0;
  string_array_seen.matched = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_read_string_array(
                     client, string_array_node, cpkt_c89_string_array_callback,
                     &string_array_seen, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(string_array_seen.count == 2);
  CPKT_C89_CHECK(string_array_seen.matched == 2);
  string_array_seen.count = 0;
  string_array_seen.matched = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_read_string_array_range(
                     client, string_array_node, "0:0",
                     cpkt_c89_string_array_callback, &string_array_seen,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(string_array_seen.count == 1);
  CPKT_C89_CHECK(string_array_seen.matched == 1);
  byte_string_array_seen.count = 0;
  byte_string_array_seen.matched = 0;
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_byte_string_array(
          client, bytes_array_node, cpkt_c89_byte_string_array_callback,
          &byte_string_array_seen, &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(byte_string_array_seen.count == 2);
  CPKT_C89_CHECK(byte_string_array_seen.matched == 2);
  byte_string_array_seen.count = 0;
  byte_string_array_seen.matched = 0;
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_byte_string_array_range(
          client, bytes_array_node, "0:0", cpkt_c89_byte_string_array_callback,
          &byte_string_array_seen, &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(byte_string_array_seen.count == 1);
  CPKT_C89_CHECK(byte_string_array_seen.matched == 1);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_access_level_ex(client, int_node, &access_level,
                                             &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_access_level_ex(client, int_node, 1UL, &status) ==
      CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_access_level_ex(client, int_node, &access_level,
                                             &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_minimum_sampling_interval(
                     client, int_node, &minimum_sampling_interval, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(minimum_sampling_interval >= 0.0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_minimum_sampling_interval(
                     client, int_node, 12.5, &status) ==
                 CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_access_level(
                     client, int_node, 1UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_access_level(client, int_node,
                                                     &access_level,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_access_level(
                     client, int_node, 3UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_user_access_level(client, int_node, &access_level,
                                               &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_historizing(
                     client, int_node, &historizing, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(historizing == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_historizing(client, int_node, 0,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_historizing(
                     client, int_node, &historizing, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(historizing == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_historizing(client, int_node, 1,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_executable(client, native_method_node,
                                                   &executable,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_write_executable(
                     client, native_method_node, 0, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_executable(client, native_method_node,
                                                   &executable,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_executable(
                     client, native_method_node, 1, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_user_executable(
                     client, native_method_node, &executable, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_event_notifier(
                     client, native_object_node, &event_notifier, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(event_notifier == 1UL);
  CPKT_C89_CHECK(cpkt_opcua_client_write_event_notifier(
                     client, native_object_node, 1UL, &status) ==
                 CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_event_notifier(
                     client, native_object_node, &event_notifier, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(event_notifier == 1UL);
  CPKT_C89_CHECK(cpkt_opcua_client_write_event_notifier(
                     client, native_object_node, 0UL, &status) ==
                 CPKT_OPCUA_ERR_UPSTREAM);
  CPKT_C89_CHECK(status != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_is_abstract(
                     client, native_reference_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 0);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_is_abstract(client, native_reference_type_node, 1,
                                          &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_is_abstract(
                     client, native_reference_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_is_abstract(client, native_reference_type_node, 0,
                                          &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_symmetric(
                     client, native_reference_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_inverse_name(
                     client, native_reference_type_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Native Related From") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_inverse_name(
                     client, native_reference_type_node, "Native Related Back",
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_inverse_name(
                     client, native_reference_type_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Native Related Back") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_inverse_name(
                     client, native_reference_type_node, "Native Related From",
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_contains_no_loops(
                     client, native_view_node, &boolean_attribute, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument_count(
                     client, native_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_INPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, native_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_INPUT, 0, &argument_data_type,
                     &argument_value_rank, name_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("input1") + 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, native_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_INPUT, 0, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "input1") == 0);
  CPKT_C89_CHECK(required == strlen("input1") + 1);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument_count(
                     client, native_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, native_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 0, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "output1") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument_count(
                     client, native_multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 2);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, native_multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 1, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "output2") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_DOUBLE);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, native_multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 2, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, native_guid_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 314);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, native_byte_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 315);
  CPKT_C89_CHECK(cpkt_opcua_client_read_browse_name(
                     client, native_object_node, &namespace_index, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(namespace_index == CPKT_OPCUA_TEST_NS);
  CPKT_C89_CHECK(strcmp(name_buffer, "nativeObject") == 0);
  CPKT_C89_CHECK(required == strlen("nativeObject") + 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_display_name(
                     client, native_object_node, name_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("Native Object") + 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_display_name(
                     client, native_object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Native Object") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_display_name(
                     client, native_object_node, "Native Object Updated",
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_display_name(
                     client, native_object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Native Object Updated") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_description(
                     client, native_object_node, name_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("Native object description") + 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_description(
                     client, native_object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Native object description") == 0);
  CPKT_C89_CHECK(
      cpkt_opcua_client_write_description(client, native_object_node,
                                          "Native object description updated",
                                          &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_description(
                     client, native_object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Native object description updated") == 0);

  CPKT_C89_CHECK(cpkt_opcua_node_id_parse("ns=1;s=nativeStringNode",
                                          &parsed_node, parsed_string_buffer,
                                          sizeof(parsed_string_buffer),
                                          &required) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(
      cpkt_opcua_node_id_equal(parsed_node, constructed_string_node) == 1);
  CPKT_C89_CHECK(cpkt_opcua_node_id_print(parsed_node, node_id_buffer, 4,
                                          &required) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("ns=1;s=nativeStringNode") + 1);
  CPKT_C89_CHECK(cpkt_opcua_node_id_print(parsed_node, node_id_buffer,
                                          sizeof(node_id_buffer),
                                          &required) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(node_id_buffer, "ns=1;s=nativeStringNode") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, parsed_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_STRING);
  CPKT_C89_CHECK(strcmp(out.string_value, "native string id") == 0);
  cpkt_opcua_value_string(&value, "string id updated",
                          strlen("string id updated"));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, parsed_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, constructed_string_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(out.string_value, "string id updated") == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_add_object(
                     client, client_added_node,
                     cpkt_opcua_node_id_numeric(0, 85), "clientAddedObject",
                     "Client Added Object", &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_integer(&value, 77);
  CPKT_C89_CHECK(cpkt_opcua_client_add_variable(
                     client, client_added_variable_node, "clientAddedVariable",
                     "Client Added Variable", &value,
                     &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_byte_string(&value, cpkt_c89_native_bytes_value,
                               sizeof(cpkt_c89_native_bytes_value));
  CPKT_C89_CHECK(cpkt_opcua_client_add_variable_under(
                     client, client_added_child_variable_node,
                     native_object_node, "clientAddedChildVariable",
                     "Client Added Child Variable", &value,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_add_object_type(
                     client, client_added_object_type_node,
                     cpkt_opcua_node_id_numeric(0, 58), "clientAddedObjectType",
                     "Client Added Object Type", 1, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_integer(&value, 88);
  CPKT_C89_CHECK(cpkt_opcua_client_add_variable_type(
                     client, client_added_variable_type_node,
                     cpkt_opcua_node_id_numeric(0, 62),
                     "clientAddedVariableType", "Client Added Variable Type",
                     &value, 0, &status) == CPKT_OPCUA_OK);
  integer_array_values[0] = 4;
  integer_array_values[1] = 5;
  integer_array_values[2] = 6;
  cpkt_opcua_value_integer_array(&value, integer_array_values, 3);
  CPKT_C89_CHECK(cpkt_opcua_client_add_variable_type(
                     client, client_added_array_variable_type_node,
                     cpkt_opcua_node_id_numeric(0, 62),
                     "clientAddedArrayVariableType",
                     "Client Added Array Variable Type", &value, 0,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_add_reference_type(
                     client, client_added_reference_type_node,
                     cpkt_opcua_node_id_numeric(0, 31),
                     "clientAddedReferenceType", "Client Added Reference Type",
                     "Client Added Reference Type Inverse", 0, 0,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_add_data_type(
                     client, client_added_data_type_node,
                     cpkt_opcua_node_id_numeric(0, 24), "clientAddedDataType",
                     "Client Added Data Type", 1, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_add_view(
                     client, client_added_view_node,
                     cpkt_opcua_node_id_numeric(0, 87), "clientAddedView",
                     "Client Added View", 1, 0, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_node_class(client, client_added_object_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_OBJECT_TYPE);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_node_class(client, client_added_variable_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VARIABLE_TYPE);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(
                     client, client_added_array_variable_type_node, &node_class,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VARIABLE_TYPE);
  CPKT_C89_CHECK(cpkt_opcua_client_read_value_rank(
                     client, client_added_array_variable_type_node, &value_rank,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(value_rank == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_array_dimensions(
                     client, client_added_array_variable_type_node, dimensions,
                     1, &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(dimensions[0] == 3UL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(
                     client, client_added_reference_type_node, &node_class,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_REFERENCE_TYPE);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_node_class(client, client_added_data_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_DATA_TYPE);
  CPKT_C89_CHECK(
      cpkt_opcua_client_read_node_class(client, client_added_view_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VIEW);
  CPKT_C89_CHECK(cpkt_opcua_client_read_is_abstract(
                     client, client_added_object_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_inverse_name(
                     client, client_added_reference_type_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Client Added Reference Type Inverse") ==
                 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_contains_no_loops(
                     client, client_added_view_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);

  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children(
                     client, cpkt_opcua_node_id_numeric(0, 85),
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.object_seen == 1);
  CPKT_C89_CHECK(browse_seen.client_added_seen == 1);
  CPKT_C89_CHECK(browse_seen.client_added_variable_seen == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, client_added_variable_node,
                                        &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 77);
  cpkt_opcua_value_integer(&value, 78);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, client_added_variable_node,
                                         &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, client_added_variable_node,
                                        &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.integer_value == 78);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children(
                     client, native_object_node, cpkt_c89_browse_callback,
                     &browse_seen, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.child_seen == 1);
  CPKT_C89_CHECK(browse_seen.client_added_child_variable_seen == 1);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.has_reference_type = 1;
  browse_options.reference_type_id = cpkt_opcua_node_id_numeric(0, 47);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_ex(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.client_added_child_variable_seen == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client,
                                        client_added_child_variable_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_BYTE_STRING);
  CPKT_C89_CHECK(out.bytes_length == sizeof(cpkt_c89_native_bytes_value));
  CPKT_C89_CHECK(memcmp(out.bytes_value, cpkt_c89_native_bytes_value,
                        out.bytes_length) == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_delete_node(client,
                                               client_added_child_variable_node,
                                               1, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_node(client,
                                               client_added_variable_node, 1,
                                               &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_node(client, client_added_node, 1,
                                               &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(client, client_added_node,
                                                   &node_class, &status) ==
                 CPKT_OPCUA_ERR_UPSTREAM);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children(
                     client, cpkt_opcua_node_id_numeric(0, 85),
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.object_seen == 1);
  CPKT_C89_CHECK(browse_seen.client_added_seen == 0);

  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children(
                     client, native_object_node, cpkt_c89_browse_callback,
                     &browse_seen, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.child_seen == 1);

  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_ex(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.child_seen == 1);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_OBJECT;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_ex(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.child_seen == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_add_reference_ex(
                     client, native_object_node,
                     cpkt_opcua_node_id_numeric(0, 47), 1,
                     cpkt_opcua_expanded_node_id_local(int_node),
                     CPKT_OPCUA_NODE_CLASS_VARIABLE, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.has_reference_type = 1;
  browse_options.reference_type_id = cpkt_opcua_node_id_numeric(0, 47);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_ex(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.int_seen == 1);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.max_references = 1;
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_page(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     small_continuation_point, sizeof(small_continuation_point),
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > sizeof(small_continuation_point));
  CPKT_C89_CHECK(browse_seen.child_seen == 0);
  CPKT_C89_CHECK(browse_seen.int_seen == 0);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_page(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen, continuation_point,
                     sizeof(continuation_point), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(required > 0);
  CPKT_C89_CHECK(browse_seen.child_seen + browse_seen.int_seen == 1);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_next(
                     client, continuation_point, required, 0,
                     cpkt_c89_browse_callback, &browse_seen,
                     next_continuation_point, sizeof(next_continuation_point),
                     &next_required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(next_required == 0);
  CPKT_C89_CHECK(browse_seen.child_seen + browse_seen.int_seen == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_reference_ex(
                     client, native_object_node,
                     cpkt_opcua_node_id_numeric(0, 47), 1,
                     cpkt_opcua_expanded_node_id_local(int_node), 1,
                     &status) == CPKT_OPCUA_OK);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_ex(
                     client, native_object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.int_seen == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, int_node, &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 41);
  CPKT_C89_CHECK(cpkt_opcua_client_read_data_value(client, int_node,
                                                   &data_value, NULL, 0, NULL,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(status == 0);
  CPKT_C89_CHECK(data_value.has_value == 1);
  CPKT_C89_CHECK(data_value.value.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(data_value.value.integer_value == 41);
  CPKT_C89_CHECK(data_value.has_server_timestamp == 1);
  cpkt_opcua_value_integer(&value, 42);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, int_node, &value, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, int_node, &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.integer_value == 42);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, guid_value_node, &out, NULL, 0,
                                        NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_GUID);
  CPKT_C89_CHECK(memcmp(out.guid_value, cpkt_c89_native_guid_node_id, 16) == 0);
  cpkt_opcua_value_guid(&value, cpkt_c89_facade_guid_node_id);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, guid_value_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, guid_value_node, &out, NULL, 0,
                                        NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(memcmp(out.guid_value, cpkt_c89_facade_guid_node_id, 16) == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_guid_array(
                     client, guid_array_node, guid_array_values, 2,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(memcmp(guid_array_values[0].bytes,
                        cpkt_c89_native_guid_node_id, 16) == 0);
  CPKT_C89_CHECK(memcmp(guid_array_values[1].bytes,
                        cpkt_c89_facade_guid_node_id, 16) == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_guid_array_range(
                     client, guid_array_node, "1", guid_array_values, 1,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(memcmp(guid_array_values[0].bytes,
                        cpkt_c89_facade_guid_node_id, 16) == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, status_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_STATUS);
  CPKT_C89_CHECK(out.status_value != 0);
  cpkt_opcua_value_status(&value, 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, status_value_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, status_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.status_value == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_status_array(
                     client, status_array_node, status_array_values, 2,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(status_array_values[0] == 0);
  CPKT_C89_CHECK(status_array_values[1] == 0x803e0000UL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_status_array_range(
                     client, status_array_node, "1", status_array_values, 1,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(status_array_values[0] == 0x803e0000UL);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, uint64_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_UINT64);
  CPKT_C89_CHECK(out.uint64_value.high32 == 0x12345678UL);
  CPKT_C89_CHECK(out.uint64_value.low32 == 0x9abcdef0UL);
  cpkt_opcua_value_uint64(&value, 0xfedcba98UL, 0x76543210UL);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, uint64_value_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, uint64_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_UINT64);
  CPKT_C89_CHECK(out.uint64_value.high32 == 0xfedcba98UL);
  CPKT_C89_CHECK(out.uint64_value.low32 == 0x76543210UL);
  cpkt_opcua_value_uint64(&value, 0xffffffffUL, 0xffffffffUL);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, uint64_value_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, uint64_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_UINT64);
  CPKT_C89_CHECK(out.uint64_value.high32 == 0xffffffffUL);
  CPKT_C89_CHECK(out.uint64_value.low32 == 0xffffffffUL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_uint64_array(
                     client, uint64_array_node, uint64_array_values, 2,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(uint64_array_values[0].high32 == 0x11111111UL);
  CPKT_C89_CHECK(uint64_array_values[0].low32 == 0x22222222UL);
  CPKT_C89_CHECK(uint64_array_values[1].high32 == 0x33333333UL);
  CPKT_C89_CHECK(uint64_array_values[1].low32 == 0x44444444UL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_uint64_array_range(
                     client, uint64_array_node, "1", uint64_array_values, 1,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(uint64_array_values[0].high32 == 0x33333333UL);
  CPKT_C89_CHECK(uint64_array_values[0].low32 == 0x44444444UL);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, datetime_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_DATETIME);
  CPKT_C89_CHECK(out.datetime_value.high32 == 0x01234567L);
  CPKT_C89_CHECK(out.datetime_value.low32 == 0x89abcdefUL);
  cpkt_opcua_value_datetime(&value, -2L, 0xfedcba98UL);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, datetime_value_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, datetime_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_DATETIME);
  CPKT_C89_CHECK(out.datetime_value.high32 == -2L);
  CPKT_C89_CHECK(out.datetime_value.low32 == 0xfedcba98UL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_datetime_array(
                     client, datetime_array_node, datetime_array_values, 3,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 3);
  CPKT_C89_CHECK(datetime_array_values[0].high32 == 0x11111111L);
  CPKT_C89_CHECK(datetime_array_values[0].low32 == 0x22222222UL);
  CPKT_C89_CHECK(datetime_array_values[1].high32 == -2L);
  CPKT_C89_CHECK(datetime_array_values[1].low32 == 0x33333333UL);
  CPKT_C89_CHECK(datetime_array_values[2].high32 == (-2147483647L - 1L));
  CPKT_C89_CHECK(datetime_array_values[2].low32 == 0x44444444UL);
  CPKT_C89_CHECK(cpkt_opcua_client_read_datetime_array_range(
                     client, datetime_array_node, "1", datetime_array_values, 1,
                     &array_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(datetime_array_values[0].high32 == -2L);
  CPKT_C89_CHECK(datetime_array_values[0].low32 == 0x33333333UL);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, qualified_name_value_node, &out,
                                        string_buffer, 4, &required,
                                        &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, qualified_name_value_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_QUALIFIED_NAME);
  CPKT_C89_CHECK(out.qualified_name_namespace_index == CPKT_OPCUA_TEST_NS);
  CPKT_C89_CHECK(strcmp(out.qualified_name, "nativeQualified") == 0);
  cpkt_opcua_value_qualified_name(&value, CPKT_OPCUA_TEST_NS, "facadeQualified",
                                  strlen("facadeQualified"));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, qualified_name_value_node,
                                         &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, qualified_name_value_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(out.qualified_name, "facadeQualified") == 0);
  memset(&qualified_name_array_seen, 0, sizeof(qualified_name_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_read_qualified_name_array(
                     client, qualified_name_array_node,
                     cpkt_c89_qualified_name_array_callback,
                     &qualified_name_array_seen, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(qualified_name_array_seen.count == 2);
  CPKT_C89_CHECK(qualified_name_array_seen.matched == 2);
  memset(&qualified_name_array_seen, 0, sizeof(qualified_name_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_read_qualified_name_array_range(
                     client, qualified_name_array_node, "1",
                     cpkt_c89_qualified_name_array_callback,
                     &qualified_name_array_seen, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(qualified_name_array_seen.count == 1);
  CPKT_C89_CHECK(qualified_name_array_seen.matched == 1);

  memset(&localized_text_array_seen, 0, sizeof(localized_text_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_read_localized_text_array(
                     client, localized_text_array_node,
                     cpkt_c89_localized_text_array_callback,
                     &localized_text_array_seen, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 2);
  CPKT_C89_CHECK(localized_text_array_seen.count == 2);
  CPKT_C89_CHECK(localized_text_array_seen.matched == 2);
  memset(&localized_text_array_seen, 0, sizeof(localized_text_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_read_localized_text_array_range(
                     client, localized_text_array_node, "1",
                     cpkt_c89_localized_text_array_callback,
                     &localized_text_array_seen, &array_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(array_count == 1);
  CPKT_C89_CHECK(localized_text_array_seen.count == 1);
  CPKT_C89_CHECK(localized_text_array_seen.matched == 1);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, localized_text_value_node, &out,
                                        string_buffer, 4, &required,
                                        &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, localized_text_value_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT);
  CPKT_C89_CHECK(strcmp(out.localized_text_locale, "sv-SE") == 0);
  CPKT_C89_CHECK(strcmp(out.localized_text, "infoddt text") == 0);
  cpkt_opcua_value_localized_text(&value, "en-US", strlen("en-US"),
                                  "facade text", strlen("facade text"));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, localized_text_value_node,
                                         &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, localized_text_value_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(out.localized_text_locale, "en-US") == 0);
  CPKT_C89_CHECK(strcmp(out.localized_text, "facade text") == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, native_child_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 7);

  cpkt_opcua_value_integer(&inputs[0], 6);
  CPKT_C89_CHECK(cpkt_opcua_client_call_method_many(
                     client, native_object_node, native_multi_method_node,
                     inputs, 1, outputs, 2, NULL, NULL, NULL,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(outputs[0].type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(outputs[0].integer_value == 12);
  CPKT_C89_CHECK(outputs[1].type == CPKT_OPCUA_VALUE_DOUBLE);
  CPKT_C89_CHECK(outputs[1].double_value > 6.49 &&
                 outputs[1].double_value < 6.51);

  for (lifecycle_iteration = 0; lifecycle_iteration < 3;
       ++lifecycle_iteration) {
    CPKT_C89_CHECK(
        cpkt_opcua_client_create_subscription(client, 50.0, &subscription_id,
                                              &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(subscription_id != 0);
    CPKT_C89_CHECK(
        cpkt_opcua_client_modify_subscription(client, subscription_id, 25.0,
                                              &status) == CPKT_OPCUA_OK);
    memset(&event_seen, 0, sizeof(event_seen));
    CPKT_C89_CHECK(cpkt_opcua_client_monitor_events(
                       client, subscription_id, native_object_node, 10.0,
                       cpkt_c89_event_callback, &event_seen,
                       &event_monitored_item_id, &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(event_monitored_item_id != 0);
    cpkt_opcua_value_integer(&inputs[0], 5);
    CPKT_C89_CHECK(cpkt_opcua_client_call_method(
                       client, native_object_node, native_method_node, inputs,
                       1, &out, NULL, 0, NULL, &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
    CPKT_C89_CHECK(out.integer_value == 15);
    for (attempt = 0; attempt < 100 && !event_seen.saw_expected; ++attempt) {
      (void)cpkt_opcua_client_run_iterate(client, 50, &status);
    }
    CPKT_C89_CHECK(event_seen.saw_expected == 1);
    CPKT_C89_CHECK(event_seen.severity == 321UL);
    CPKT_C89_CHECK(event_seen.event_id_length != 0);
    CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                       client, subscription_id, event_monitored_item_id,
                       &status) == CPKT_OPCUA_OK);
    event_field_names[0].data = "EventId";
    event_field_names[0].length = strlen("EventId");
    event_field_names[1].data = "Message";
    event_field_names[1].length = strlen("Message");
    event_field_names[2].data = "Severity";
    event_field_names[2].length = strlen("Severity");
    memset(&event_seen, 0, sizeof(event_seen));
    CPKT_C89_CHECK(cpkt_opcua_client_monitor_event_fields(
                       client, subscription_id, native_object_node, 10.0,
                       event_field_names, 3, cpkt_c89_event_fields_callback,
                       &event_seen, &event_monitored_item_id,
                       &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(event_monitored_item_id != 0);
    cpkt_opcua_value_integer(&inputs[0], 5);
    CPKT_C89_CHECK(cpkt_opcua_client_call_method(
                       client, native_object_node, native_method_node, inputs,
                       1, &out, NULL, 0, NULL, &status) == CPKT_OPCUA_OK);
    for (attempt = 0; attempt < 100 && !event_seen.saw_expected_fields;
         ++attempt) {
      (void)cpkt_opcua_client_run_iterate(client, 50, &status);
    }
    CPKT_C89_CHECK(event_seen.saw_expected_fields == 1);
    CPKT_C89_CHECK(event_seen.field_severity == 321L);
    CPKT_C89_CHECK(strcmp(event_seen.field_message, "native method event") ==
                   0);
    CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                       client, subscription_id, event_monitored_item_id,
                       &status) == CPKT_OPCUA_OK);
    memset(&subscription_seen, 0, sizeof(subscription_seen));
    CPKT_C89_CHECK(cpkt_opcua_client_monitor_value(
                       client, subscription_id, int_node, 10.0,
                       cpkt_c89_subscription_callback, &subscription_seen,
                       &monitored_item_id, &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(monitored_item_id != 0);
    CPKT_C89_CHECK(cpkt_opcua_client_set_monitoring_mode(
                       client, subscription_id, monitored_item_id,
                       CPKT_OPCUA_MONITORING_SAMPLING,
                       &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(cpkt_opcua_client_set_monitoring_mode(
                       client, subscription_id, monitored_item_id,
                       CPKT_OPCUA_MONITORING_REPORTING,
                       &status) == CPKT_OPCUA_OK);
    cpkt_opcua_value_integer(&value, 43);
    CPKT_C89_CHECK(cpkt_opcua_client_write(client, int_node, &value, &status) ==
                   CPKT_OPCUA_OK);
    for (attempt = 0; attempt < 100 && !subscription_seen.saw_expected;
         ++attempt) {
      (void)cpkt_opcua_client_run_iterate(client, 50, &status);
    }
    CPKT_C89_CHECK(subscription_seen.saw_expected == 1);
    CPKT_C89_CHECK(subscription_seen.last_integer == 43);
    CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                       client, subscription_id, monitored_item_id, &status) ==
                   CPKT_OPCUA_OK);
    memset(&subscription_seen, 0, sizeof(subscription_seen));
    cpkt_opcua_monitor_options_default(&monitor_options);
    monitor_options.sampling_interval_ms = 10.0;
    monitor_options.queue_size = 2;
    monitor_options.discard_oldest = 0;
    monitor_options.deadband_type = CPKT_OPCUA_DEADBAND_ABSOLUTE;
    monitor_options.deadband_value = 0.0;
    CPKT_C89_CHECK(cpkt_opcua_client_monitor_value_ex(
                       client, subscription_id, int_node, &monitor_options,
                       cpkt_c89_subscription_callback, &subscription_seen,
                       &monitored_item_id, &status) == CPKT_OPCUA_OK);
    CPKT_C89_CHECK(monitored_item_id != 0);
    cpkt_opcua_value_integer(&value, 44);
    CPKT_C89_CHECK(cpkt_opcua_client_write(client, int_node, &value, &status) ==
                   CPKT_OPCUA_OK);
    for (attempt = 0; attempt < 100 && !subscription_seen.saw_expected;
         ++attempt) {
      (void)cpkt_opcua_client_run_iterate(client, 50, &status);
    }
    CPKT_C89_CHECK(subscription_seen.saw_expected == 1);
    CPKT_C89_CHECK(subscription_seen.last_integer == 44);
    CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                       client, subscription_id, monitored_item_id, &status) ==
                   CPKT_OPCUA_OK);
    CPKT_C89_CHECK(cpkt_opcua_client_delete_subscription(
                       client, subscription_id, &status) == CPKT_OPCUA_OK);
  }

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, bool_node, &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_BOOLEAN);
  CPKT_C89_CHECK(out.boolean_value == 1);
  cpkt_opcua_value_boolean(&value, 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, bool_node, &value, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, bool_node, &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.boolean_value == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, bytes_node, &out, string_buffer,
                                        2, &required,
                                        &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == sizeof(cpkt_c89_native_bytes_value));
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, bytes_node, &out, string_buffer,
                                        sizeof(string_buffer), &required,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_BYTE_STRING);
  CPKT_C89_CHECK(out.bytes_length == sizeof(cpkt_c89_native_bytes_value));
  CPKT_C89_CHECK(memcmp(out.bytes_value, cpkt_c89_native_bytes_value,
                        out.bytes_length) == 0);
  cpkt_opcua_value_byte_string(&value, cpkt_c89_native_bytes_updated,
                               sizeof(cpkt_c89_native_bytes_updated));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, bytes_node, &value, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, bytes_node, &out, string_buffer,
                                        sizeof(string_buffer), &required,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_BYTE_STRING);
  CPKT_C89_CHECK(out.bytes_length == sizeof(cpkt_c89_native_bytes_updated));
  CPKT_C89_CHECK(memcmp(out.bytes_value, cpkt_c89_native_bytes_updated,
                        out.bytes_length) == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_create_subscription(
                     client, 50.0, &subscription_id, &status) == CPKT_OPCUA_OK);
  memset(&subscription_seen, 0, sizeof(subscription_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_monitor_value(
                     client, subscription_id, bytes_node, 10.0,
                     cpkt_c89_subscription_callback, &subscription_seen,
                     &monitored_item_id, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_byte_string(&value, cpkt_c89_native_bytes_updated,
                               sizeof(cpkt_c89_native_bytes_updated));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, bytes_node, &value, &status) ==
                 CPKT_OPCUA_OK);
  for (attempt = 0; attempt < 100 && !subscription_seen.saw_expected_bytes;
       ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  CPKT_C89_CHECK(subscription_seen.saw_expected_bytes == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                     client, subscription_id, monitored_item_id, &status) ==
                 CPKT_OPCUA_OK);

  memset(&subscription_seen, 0, sizeof(subscription_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_monitor_value(
                     client, subscription_id, qualified_name_value_node, 10.0,
                     cpkt_c89_subscription_callback, &subscription_seen,
                     &monitored_item_id, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_qualified_name(&value, CPKT_OPCUA_TEST_NS,
                                  "monitorQualified",
                                  strlen("monitorQualified"));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, qualified_name_value_node,
                                         &value, &status) == CPKT_OPCUA_OK);
  for (attempt = 0;
       attempt < 100 && !subscription_seen.saw_expected_qualified_name;
       ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  CPKT_C89_CHECK(subscription_seen.saw_expected_qualified_name == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                     client, subscription_id, monitored_item_id, &status) ==
                 CPKT_OPCUA_OK);

  memset(&subscription_seen, 0, sizeof(subscription_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_monitor_value(
                     client, subscription_id, localized_text_value_node, 10.0,
                     cpkt_c89_subscription_callback, &subscription_seen,
                     &monitored_item_id, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_localized_text(&value, "en-US", strlen("en-US"),
                                  "monitor text", strlen("monitor text"));
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, localized_text_value_node,
                                         &value, &status) == CPKT_OPCUA_OK);
  for (attempt = 0;
       attempt < 100 && !subscription_seen.saw_expected_localized_text;
       ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  CPKT_C89_CHECK(subscription_seen.saw_expected_localized_text == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                     client, subscription_id, monitored_item_id, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_subscription(
                     client, subscription_id, &status) == CPKT_OPCUA_OK);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, string_node, &out,
                                        string_buffer, 4, &required,
                                        &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, string_node, &out,
                                        string_buffer, sizeof(string_buffer),
                                        &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_STRING);
  CPKT_C89_CHECK(strcmp(out.string_value, "native") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_read_data_value(
                     client, string_node, &data_value, string_buffer, 4,
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("native") + 1);
  CPKT_C89_CHECK(data_value.has_value == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_read(
                     client,
                     cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 9999), &out,
                     NULL, 0, NULL, &status) == CPKT_OPCUA_ERR_UPSTREAM);

  CPKT_C89_CHECK(cpkt_opcua_client_disconnect(client, &status) ==
                 CPKT_OPCUA_OK);
  cpkt_opcua_client_free(client);
  return 0;
}

int cpkt_opcua_c89_client_exercise_facade_server(const char *endpoint) {
  cpkt_opcua_client *client;
  cpkt_opcua_node_id int_node;
  cpkt_opcua_node_id object_node;
  cpkt_opcua_node_id child_node;
  cpkt_opcua_node_id method_node;
  cpkt_opcua_node_id multi_method_node;
  cpkt_opcua_node_id argument_data_type;
  cpkt_opcua_node_id uint64_value_node;
  cpkt_opcua_node_id array_node;
  cpkt_opcua_node_id translated_node;
  cpkt_opcua_browse_path_element child_path[2];
  cpkt_opcua_browse_options browse_options;
  struct cpkt_c89_browse_seen browse_seen;
  struct cpkt_c89_subscription_seen subscription_seen;
  struct cpkt_c89_async_seen async_seen;
  cpkt_opcua_value value;
  cpkt_opcua_value out;
  cpkt_opcua_value inputs[1];
  cpkt_opcua_value outputs[2];
  cpkt_opcua_node_id async_node_out;
  cpkt_opcua_uint64 uint64_value;
  long integer_values[3];
  cpkt_opcua_status status;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  cpkt_opcua_request_id request_id;
  char string_buffer[64];
  char endpoint_buffer[128];
  char namespace_buffer[64];
  char server_buffer[128];
  unsigned long node_class;
  unsigned short namespace_index;
  size_t required;
  size_t count;
  size_t endpoint_count;
  size_t server_count;
  size_t argument_count;
  long argument_value_rank;
  int executable;
  int attempt;

  CPKT_C89_CHECK(endpoint != NULL);
  client = NULL;
  CPKT_C89_CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_connect(client, endpoint, &status) ==
                 CPKT_OPCUA_OK);

  CPKT_C89_CHECK(
      cpkt_opcua_client_get_endpoint_count(client, endpoint, &endpoint_count,
                                           &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(endpoint_count > 0);
  CPKT_C89_CHECK(cpkt_opcua_client_get_endpoint_url(
                     client, endpoint, 0, endpoint_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(
      cpkt_opcua_client_get_endpoint_url(client, endpoint, 0, endpoint_buffer,
                                         sizeof(endpoint_buffer), &required,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(strncmp(endpoint_buffer, "opc.tcp://", strlen("opc.tcp://")) ==
                 0);
  CPKT_C89_CHECK(cpkt_opcua_client_get_endpoint_url(
                     client, endpoint, endpoint_count, endpoint_buffer,
                     sizeof(endpoint_buffer), &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_count(client, endpoint,
                                                     &server_count,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(server_count > 0);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_uri(
                     client, endpoint, 0, server_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > 4);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_uri(
                     client, endpoint, 0, server_buffer, sizeof(server_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(server_buffer, CPKT_OPCUA_FACADE_APPLICATION_URI) == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_name(
                     client, endpoint, 0, server_buffer, sizeof(server_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(server_buffer, CPKT_OPCUA_FACADE_APPLICATION_NAME) ==
                 0);
  CPKT_C89_CHECK(cpkt_opcua_client_find_server_application_name(
                     client, endpoint, server_count, server_buffer,
                     sizeof(server_buffer), &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(cpkt_opcua_client_get_namespace_index(
                     client, CPKT_OPCUA_FACADE_NAMESPACE_URI, &namespace_index,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(namespace_index != 0);
  CPKT_C89_CHECK(cpkt_opcua_client_get_namespace_uri(
                     client, namespace_index, namespace_buffer,
                     sizeof(namespace_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(namespace_buffer, CPKT_OPCUA_FACADE_NAMESPACE_URI) ==
                 0);

  int_node =
      cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID);
  object_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_FACADE_OBJECT_ID);
  child_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_FACADE_CHILD_ID);
  method_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_FACADE_METHOD_ID);
  multi_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_MULTI_METHOD_ID);
  uint64_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_UINT64_VALUE_ID);
  array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_FACADE_ARRAY_ID);

  CPKT_C89_CHECK(cpkt_opcua_client_read_node_class(client, object_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_OBJECT);
  CPKT_C89_CHECK(cpkt_opcua_client_read_display_name(
                     client, object_node, string_buffer, sizeof(string_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(string_buffer, "Facade Object Updated") == 0);

  CPKT_C89_CHECK(cpkt_opcua_client_read(client, int_node, &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 5);
  cpkt_opcua_value_integer(&value, 43);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, int_node, &value, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, int_node, &out, NULL, 0, NULL,
                                        &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 43);

  CPKT_C89_CHECK(cpkt_opcua_client_read_uint64_array_range(
                     client,
                     cpkt_opcua_node_id_numeric(
                         CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_UINT64_ARRAY_ID),
                     "1", &uint64_value, 1, &count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(count == 1);
  CPKT_C89_CHECK(uint64_value.high32 == 0x33333333UL);
  CPKT_C89_CHECK(uint64_value.low32 == 0x44444444UL);
  cpkt_opcua_value_uint64(&value, 0xffffffffUL, 0xffffffffUL);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, uint64_value_node, &value,
                                         &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read(client, uint64_value_node, &out, NULL,
                                        0, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_UINT64);
  CPKT_C89_CHECK(out.uint64_value.high32 == 0xffffffffUL);
  CPKT_C89_CHECK(out.uint64_value.low32 == 0xffffffffUL);

  CPKT_C89_CHECK(cpkt_opcua_client_read_integer_array(
                     client, array_node, integer_values, 3, &count, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(count == 3);
  cpkt_opcua_value_integer_array(&value, integer_values + 1, 2);
  integer_values[1] = 31;
  integer_values[2] = 32;
  CPKT_C89_CHECK(cpkt_opcua_client_write_index_range(client, array_node, "1:2",
                                                     &value,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_integer_array_range(
                     client, array_node, "1:2", integer_values, 2, &count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(count == 2);
  CPKT_C89_CHECK(integer_values[0] == 31);
  CPKT_C89_CHECK(integer_values[1] == 32);

  memset(&browse_seen, 0, sizeof(browse_seen));
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.max_references = 0;
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_ex(
                     client, cpkt_opcua_node_id_numeric(0, 85), &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.object_seen == 1);
  child_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  child_path[0].browse_name = "facadeObject";
  child_path[1].namespace_index = CPKT_OPCUA_TEST_NS;
  child_path[1].browse_name = "facadeObjectChild";
  CPKT_C89_CHECK(cpkt_opcua_client_translate_browse_path(
                     client, cpkt_opcua_node_id_numeric(0, 85), child_path, 2,
                     &translated_node, string_buffer, sizeof(string_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_node_id_equal(translated_node, child_node) == 1);

  CPKT_C89_CHECK(cpkt_opcua_client_read_executable(client, method_node,
                                                   &executable,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_user_executable(client, method_node,
                                                        &executable, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_write_executable(client, method_node, 0,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_executable(client, method_node,
                                                   &executable,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_write_executable(client, method_node, 1,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument_count(
                     client, method_node, CPKT_OPCUA_METHOD_ARGUMENT_INPUT,
                     &argument_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, method_node, CPKT_OPCUA_METHOD_ARGUMENT_INPUT, 0,
                     &argument_data_type, &argument_value_rank, string_buffer,
                     4, &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("input1") + 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, method_node, CPKT_OPCUA_METHOD_ARGUMENT_INPUT, 0,
                     &argument_data_type, &argument_value_rank, string_buffer,
                     sizeof(string_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(string_buffer, "input1") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument_count(
                     client, method_node, CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT,
                     &argument_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, method_node, CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 0,
                     &argument_data_type, &argument_value_rank, string_buffer,
                     sizeof(string_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(string_buffer, "output1") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument_count(
                     client, multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 2);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 1, &argument_data_type,
                     &argument_value_rank, string_buffer, sizeof(string_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(string_buffer, "output2") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_DOUBLE);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_client_read_method_argument(
                     client, multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 2, &argument_data_type,
                     &argument_value_rank, string_buffer, sizeof(string_buffer),
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);

  cpkt_opcua_value_integer(&inputs[0], 7);
  CPKT_C89_CHECK(cpkt_opcua_client_call_method(client, object_node, method_node,
                                               inputs, 1, &out, NULL, 0, NULL,
                                               &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(out.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(out.integer_value == 21);
  CPKT_C89_CHECK(cpkt_opcua_client_call_method_many(
                     client, object_node, multi_method_node, inputs, 1, outputs,
                     2, NULL, NULL, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(outputs[0].type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(outputs[0].integer_value == 8);
  CPKT_C89_CHECK(outputs[1].type == CPKT_OPCUA_VALUE_DOUBLE);
  CPKT_C89_CHECK(outputs[1].double_value > 7.24 &&
                 outputs[1].double_value < 7.26);

  memset(&async_seen, 0, sizeof(async_seen));
  cpkt_opcua_value_integer(&value, 57);
  CPKT_C89_CHECK(cpkt_opcua_client_write_async(
                     client, int_node, &value, cpkt_c89_async_status_callback,
                     &async_seen, &request_id, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.status == 0);

  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_read_async(client, int_node,
                                              cpkt_c89_async_value_callback,
                                              &async_seen, &request_id, NULL, 0,
                                              NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.integer_value == 57);

  cpkt_opcua_value_integer(&inputs[0], 8);
  memset(outputs, 0, sizeof(outputs));
  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_call_method_async(
                     client, object_node, multi_method_node, inputs, 1, 2,
                     cpkt_c89_async_call_callback, &async_seen, &request_id,
                     outputs, NULL, NULL, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.integer_value == 9);
  CPKT_C89_CHECK(async_seen.double_value > 8.24 &&
                 async_seen.double_value < 8.26);

  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_browse_children_async(
                     client, cpkt_opcua_node_id_numeric(0, 85), NULL,
                     cpkt_c89_async_browse_entry_callback,
                     cpkt_c89_async_browse_done_callback, &async_seen,
                     &request_id, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.browse_seen == 1);

  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_add_object_async(
                     client,
                     cpkt_opcua_node_id_numeric(
                         CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ASYNC_OBJECT_ID),
                     object_node, "facadeAsyncObject", "Facade Async Object",
                     cpkt_c89_async_node_callback, &async_seen, &request_id,
                     &async_node_out, string_buffer, sizeof(string_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.node_seen == 1);
  CPKT_C89_CHECK(async_seen.node_numeric == CPKT_OPCUA_FACADE_ASYNC_OBJECT_ID);
  CPKT_C89_CHECK(async_node_out.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(async_node_out.numeric == CPKT_OPCUA_FACADE_ASYNC_OBJECT_ID);

  cpkt_opcua_value_integer(&value, 58);
  memset(&async_seen, 0, sizeof(async_seen));
  CPKT_C89_CHECK(
      cpkt_opcua_client_add_variable_async(
          client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                     CPKT_OPCUA_FACADE_ASYNC_VARIABLE_ID),
          object_node, "facadeAsyncVariable", "Facade Async Variable", &value,
          cpkt_c89_async_node_callback, &async_seen, &request_id,
          &async_node_out, string_buffer, sizeof(string_buffer), &required,
          &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(request_id != 0);
  cpkt_c89_wait_for_async(client, &async_seen);
  CPKT_C89_CHECK(async_seen.done == 1);
  CPKT_C89_CHECK(async_seen.result == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(async_seen.node_seen == 1);
  CPKT_C89_CHECK(async_seen.node_numeric ==
                 CPKT_OPCUA_FACADE_ASYNC_VARIABLE_ID);
  CPKT_C89_CHECK(async_node_out.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(async_node_out.numeric == CPKT_OPCUA_FACADE_ASYNC_VARIABLE_ID);

  memset(&subscription_seen, 0, sizeof(subscription_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_create_subscription(
                     client, 20.0, &subscription_id, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_monitor_value(
                     client, subscription_id, int_node, 10.0,
                     cpkt_c89_subscription_callback, &subscription_seen,
                     &monitored_item_id, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_integer(&value, 44);
  CPKT_C89_CHECK(cpkt_opcua_client_write(client, int_node, &value, &status) ==
                 CPKT_OPCUA_OK);
  for (attempt = 0; attempt < 100 && !subscription_seen.saw_expected;
       ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  CPKT_C89_CHECK(subscription_seen.saw_expected == 1);
  CPKT_C89_CHECK(subscription_seen.last_status == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                     client, subscription_id, monitored_item_id, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_subscription(
                     client, subscription_id, &status) == CPKT_OPCUA_OK);

  CPKT_C89_CHECK(cpkt_opcua_client_disconnect(client, &status) ==
                 CPKT_OPCUA_OK);
  cpkt_opcua_client_free(client);
  return 0;
}

int cpkt_opcua_c89_client_monitor_facade_server_events(
    const char *endpoint, cpkt_opcua_c89_trigger_fn trigger_fn,
    void *trigger_user) {
  cpkt_opcua_client *client;
  cpkt_opcua_node_id object_node;
  cpkt_opcua_status status;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  cpkt_opcua_string_view event_field_names[3];
  struct cpkt_c89_event_seen event_seen;
  int attempt;

  CPKT_C89_CHECK(endpoint != NULL);
  CPKT_C89_CHECK(trigger_fn != NULL);
  client = NULL;
  CPKT_C89_CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_connect(client, endpoint, &status) ==
                 CPKT_OPCUA_OK);
  object_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_FACADE_OBJECT_ID);

  CPKT_C89_CHECK(cpkt_opcua_client_write_event_notifier(
                     client, object_node, 1UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_create_subscription(
                     client, 20.0, &subscription_id, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(subscription_id != 0);

  memset(&event_seen, 0, sizeof(event_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_monitor_events(
                     client, subscription_id, object_node, 10.0,
                     cpkt_c89_event_callback, &event_seen, &monitored_item_id,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(monitored_item_id != 0);
  CPKT_C89_CHECK(trigger_fn(trigger_user) == 0);
  for (attempt = 0; attempt < 100 && !event_seen.saw_expected; ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  CPKT_C89_CHECK(event_seen.saw_expected == 1);
  CPKT_C89_CHECK(event_seen.severity == 654UL);
  CPKT_C89_CHECK(event_seen.event_id_length != 0);
  CPKT_C89_CHECK(strcmp(event_seen.message, "facade server event") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                     client, subscription_id, monitored_item_id, &status) ==
                 CPKT_OPCUA_OK);

  event_field_names[0].data = "EventId";
  event_field_names[0].length = strlen("EventId");
  event_field_names[1].data = "Message";
  event_field_names[1].length = strlen("Message");
  event_field_names[2].data = "Severity";
  event_field_names[2].length = strlen("Severity");
  memset(&event_seen, 0, sizeof(event_seen));
  CPKT_C89_CHECK(cpkt_opcua_client_monitor_event_fields(
                     client, subscription_id, object_node, 10.0,
                     event_field_names, 3, cpkt_c89_event_fields_callback,
                     &event_seen, &monitored_item_id,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(monitored_item_id != 0);
  CPKT_C89_CHECK(trigger_fn(trigger_user) == 0);
  for (attempt = 0; attempt < 100 && !event_seen.saw_expected_fields;
       ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  CPKT_C89_CHECK(event_seen.saw_expected_fields == 1);
  CPKT_C89_CHECK(event_seen.field_severity == 654L);
  CPKT_C89_CHECK(strcmp(event_seen.field_message, "facade server event") == 0);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_monitored_item(
                     client, subscription_id, monitored_item_id, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_delete_subscription(
                     client, subscription_id, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_write_event_notifier(
                     client, object_node, 0UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_client_disconnect(client, &status) ==
                 CPKT_OPCUA_OK);
  cpkt_opcua_client_free(client);
  return 0;
}

int cpkt_opcua_c89_server_peer_new(unsigned short port,
                                   cpkt_opcua_c89_server_peer **out,
                                   char *endpoint_buffer,
                                   unsigned long endpoint_buffer_size) {
  cpkt_opcua_c89_server_peer *peer;
  cpkt_opcua_node_id object_node;
  cpkt_opcua_node_id child_node;
  cpkt_opcua_node_id deleted_object_node;
  cpkt_opcua_node_id data_type_node;
  cpkt_opcua_node_id translated_node;
  cpkt_opcua_node_id argument_data_type;
  cpkt_opcua_node_id method_node;
  cpkt_opcua_node_id echo_method_node;
  cpkt_opcua_node_id qualified_name_method_node;
  cpkt_opcua_node_id localized_text_method_node;
  cpkt_opcua_node_id empty_method_node;
  cpkt_opcua_node_id multi_method_node;
  cpkt_opcua_node_id object_type_node;
  cpkt_opcua_node_id variable_type_node;
  cpkt_opcua_node_id array_variable_type_node;
  cpkt_opcua_node_id reference_type_node;
  cpkt_opcua_node_id view_node;
  cpkt_opcua_node_id int_node;
  cpkt_opcua_node_id double_node;
  cpkt_opcua_node_id string_node;
  cpkt_opcua_node_id bytes_node;
  cpkt_opcua_node_id array_node;
  cpkt_opcua_node_id bool_array_node;
  cpkt_opcua_node_id double_array_node;
  cpkt_opcua_node_id string_array_node;
  cpkt_opcua_node_id bytes_array_node;
  cpkt_opcua_node_id guid_value_node;
  cpkt_opcua_node_id guid_array_node;
  cpkt_opcua_node_id status_value_node;
  cpkt_opcua_node_id status_array_node;
  cpkt_opcua_node_id uint64_value_node;
  cpkt_opcua_node_id uint64_array_node;
  cpkt_opcua_node_id datetime_value_node;
  cpkt_opcua_node_id datetime_array_node;
  cpkt_opcua_node_id qualified_name_value_node;
  cpkt_opcua_node_id qualified_name_array_node;
  cpkt_opcua_node_id localized_text_value_node;
  cpkt_opcua_node_id localized_text_array_node;
  cpkt_opcua_node_id pubsub_connection_node;
  cpkt_opcua_node_id pubsub_dataset_node;
  cpkt_opcua_node_id pubsub_field_node;
  cpkt_opcua_node_id pubsub_writer_group_node;
  cpkt_opcua_node_id pubsub_writer_node;
  cpkt_opcua_node_id pubsub_reader_group_node;
  cpkt_opcua_node_id pubsub_reader_node;
  cpkt_opcua_node_id guid_node;
  cpkt_opcua_node_id byte_node;
  cpkt_opcua_browse_path_element object_child_path[2];
  cpkt_opcua_browse_path_element string_path[1];
  cpkt_opcua_browse_path_element missing_path[1];
  cpkt_opcua_value value;
  cpkt_opcua_data_value data_value;
  cpkt_opcua_guid guid_array_values[2];
  cpkt_opcua_status status_array_values[2];
  int bool_array_values[2];
  long array_values[3];
  cpkt_opcua_uint64 uint64_array_values[2];
  cpkt_opcua_datetime datetime_array_values[3];
  double double_array_values[2];
  cpkt_opcua_string_view string_array_values[2];
  cpkt_opcua_byte_string_view byte_string_array_values[2];
  cpkt_opcua_qualified_name_view qualified_name_array_values[2];
  cpkt_opcua_localized_text_view localized_text_array_values[2];
  cpkt_opcua_mqtt_connection_options mqtt_options;
  cpkt_opcua_pubsub_writer_group_options writer_group_options;
  cpkt_opcua_pubsub_data_set_writer_options writer_options;
  cpkt_opcua_pubsub_reader_group_options reader_group_options;
  cpkt_opcua_pubsub_data_set_reader_options reader_options;
  unsigned char byte_string_array_0[2];
  unsigned char byte_string_array_1[3];
  struct cpkt_c89_string_array_seen string_array_seen;
  struct cpkt_c89_byte_string_array_seen byte_string_array_seen;
  struct cpkt_c89_qualified_name_array_seen qualified_name_array_seen;
  struct cpkt_c89_localized_text_array_seen localized_text_array_seen;
  cpkt_opcua_status status;
  struct cpkt_c89_browse_seen browse_seen;
  cpkt_opcua_browse_options browse_options;
  size_t required;
  size_t next_required;
  size_t argument_count;
  size_t dimension_count;
  unsigned short namespace_index;
  unsigned short browse_namespace_index;
  unsigned long node_class;
  unsigned long access_level;
  unsigned long write_mask;
  unsigned long original_write_mask;
  unsigned long event_notifier;
  unsigned long dimensions[2];
  long value_rank;
  long argument_value_rank;
  double minimum_sampling_interval;
  int executable;
  int historizing;
  int boolean_attribute;
  int native_seen;
  int native_variant_seen;
  int native_data_value_seen;
  static const unsigned char json_config[] =
      "{ applicationDescription: { applicationUri: \"urn:cpkt:opcua:c89-json\" "
      "} }";
  char name_buffer[64];
  unsigned char continuation_point[64];
  unsigned char next_continuation_point[64];
  unsigned char small_continuation_point[1];
  int method_input_types[1];
  int string_method_input_types[1];
  int bytes_method_input_types[1];
  int qualified_name_method_input_types[1];
  int localized_text_method_input_types[1];
  int multi_output_types[2];
  static long method_factor = 3;

  CPKT_C89_CHECK(out != NULL);
  *out = NULL;
  peer = (cpkt_opcua_c89_server_peer *)calloc(1, sizeof(*peer));
  CPKT_C89_CHECK(peer != NULL);
  if (cpkt_opcua_server_new_from_json(&peer->server, json_config,
                                      sizeof(json_config) - 1,
                                      &status) != CPKT_OPCUA_OK) {
    free(peer);
    return CPKT_C89_FAIL(__LINE__);
  }
  CPKT_C89_CHECK(cpkt_opcua_server_set_endpoint(peer->server, "127.0.0.1",
                                                port) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_set_application_identity(
                     peer->server, CPKT_OPCUA_FACADE_APPLICATION_URI,
                     CPKT_OPCUA_FACADE_PRODUCT_URI,
                     CPKT_OPCUA_FACADE_APPLICATION_NAME) == CPKT_OPCUA_OK);
  native_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_file_config_native_config(
                     peer->server, cpkt_c89_native_config_seen, &native_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  native_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_security_plugin_native_config(
                     peer->server, cpkt_c89_native_config_seen, &native_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_set_access_control_callback(
                     peer->server, 1, cpkt_c89_login_callback, NULL, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_endpoint_url(peer->server, endpoint_buffer,
                                                (size_t)endpoint_buffer_size,
                                                &required) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_namespace(
                     peer->server, CPKT_OPCUA_FACADE_NAMESPACE_URI,
                     &namespace_index) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(namespace_index != 0);

  int_node =
      cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID);
  double_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_FACADE_DOUBLE_ID);
  string_node = cpkt_opcua_node_id_string(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_FACADE_STRING_NODE_ID);
  bytes_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_FACADE_BYTES_ID);
  array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_FACADE_ARRAY_ID);
  bool_array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                               CPKT_OPCUA_FACADE_BOOL_ARRAY_ID);
  double_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DOUBLE_ARRAY_ID);
  string_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_STRING_ARRAY_ID);
  bytes_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_ARRAY_ID);
  guid_value_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                               CPKT_OPCUA_FACADE_GUID_VALUE_ID);
  guid_array_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                               CPKT_OPCUA_FACADE_GUID_ARRAY_ID);
  status_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_STATUS_VALUE_ID);
  status_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_STATUS_ARRAY_ID);
  uint64_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_UINT64_VALUE_ID);
  uint64_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_UINT64_ARRAY_ID);
  datetime_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DATETIME_VALUE_ID);
  datetime_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DATETIME_ARRAY_ID);
  qualified_name_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_QUALIFIED_NAME_VALUE_ID);
  qualified_name_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_QUALIFIED_NAME_ARRAY_ID);
  localized_text_value_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_LOCALIZED_TEXT_VALUE_ID);
  localized_text_array_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_LOCALIZED_TEXT_ARRAY_ID);
  object_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_FACADE_OBJECT_ID);
  child_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                          CPKT_OPCUA_FACADE_CHILD_ID);
  deleted_object_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DELETED_OBJECT_ID);
  method_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                           CPKT_OPCUA_FACADE_METHOD_ID);
  echo_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ECHO_METHOD_ID);
  qualified_name_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_QUALIFIED_NAME_METHOD_ID);
  localized_text_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_LOCALIZED_TEXT_METHOD_ID);
  empty_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_EMPTY_METHOD_ID);
  multi_method_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_MULTI_METHOD_ID);
  object_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_TYPE_ID);
  variable_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_VARIABLE_TYPE_ID);
  array_variable_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ARRAY_VARIABLE_TYPE_ID);
  reference_type_node = cpkt_opcua_node_id_numeric(
      CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_REFERENCE_TYPE_ID);
  data_type_node = cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                              CPKT_OPCUA_FACADE_DATA_TYPE_ID);
  view_node =
      cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_VIEW_ID);
  guid_node =
      cpkt_opcua_node_id_guid(CPKT_OPCUA_TEST_NS, cpkt_c89_facade_guid_node_id);
  byte_node = cpkt_opcua_node_id_byte_string(
      CPKT_OPCUA_TEST_NS, cpkt_c89_facade_byte_node_id,
      sizeof(cpkt_c89_facade_byte_node_id));

  cpkt_opcua_value_integer(&value, 5);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, int_node, "facadeInteger", "Facade Integer",
                     &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_data_value(peer->server, int_node,
                                                   &data_value, NULL, 0, NULL,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(status == 0);
  CPKT_C89_CHECK(data_value.has_value == 1);
  CPKT_C89_CHECK(data_value.value.type == CPKT_OPCUA_VALUE_INTEGER);
  CPKT_C89_CHECK(data_value.value.integer_value == 5);
  CPKT_C89_CHECK(data_value.has_server_timestamp == 1);
  native_variant_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_read_native_variant(
                     peer->server, int_node, cpkt_c89_native_variant_seen,
                     &native_variant_seen, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_variant_seen == 1);
  native_data_value_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_read_native_data_value(
                     peer->server, int_node, cpkt_c89_native_data_value_seen,
                     &native_data_value_seen, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_data_value_seen == 1);
  cpkt_opcua_mqtt_connection_options_default(&mqtt_options);
  mqtt_options.name = "c89 mqtt";
  mqtt_options.broker_host = "127.0.0.1";
  mqtt_options.topic = "cpkt/opcua/c89";
  mqtt_options.publisher_id = 24;
  mqtt_options.validate_only = 1;
  mqtt_options.enabled = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_add_mqtt_pubsub_connection(
                     peer->server, &mqtt_options, &pubsub_connection_node,
                     name_buffer, sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(pubsub_connection_node.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(cpkt_opcua_server_add_published_dataset(
                     peer->server, "c89 pubsub dataset", &pubsub_dataset_node,
                     name_buffer, sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_published_variable(
                     peer->server, pubsub_dataset_node, int_node, "c89Field",
                     &pubsub_field_node, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_writer_group_options_default(&writer_group_options);
  writer_group_options.name = "c89 writer group";
  writer_group_options.writer_group_id = 24;
  writer_group_options.enabled = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_add_pubsub_writer_group(
                     peer->server, pubsub_connection_node,
                     &writer_group_options, &pubsub_writer_group_node,
                     name_buffer, sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_data_set_writer_options_default(&writer_options);
  writer_options.name = "c89 writer";
  writer_options.data_set_writer_id = 25;
  writer_options.enabled = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_add_pubsub_data_set_writer(
                     peer->server, pubsub_writer_group_node,
                     pubsub_dataset_node, &writer_options, &pubsub_writer_node,
                     name_buffer, sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_reader_group_options_default(&reader_group_options);
  reader_group_options.name = "c89 reader group";
  reader_group_options.enabled = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_add_pubsub_reader_group(
                     peer->server, pubsub_connection_node,
                     &reader_group_options, &pubsub_reader_group_node,
                     name_buffer, sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_data_set_reader_options_default(&reader_options);
  reader_options.name = "c89 reader";
  reader_options.publisher_id = 24;
  reader_options.writer_group_id = 24;
  reader_options.data_set_writer_id = 25;
  reader_options.enabled = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_add_pubsub_data_set_reader(
                     peer->server, pubsub_reader_group_node, &reader_options,
                     &pubsub_reader_node, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  (void)pubsub_field_node;
  CPKT_C89_CHECK(pubsub_writer_node.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(pubsub_reader_node.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  cpkt_opcua_value_double(&value, 3.5);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, double_node, "facadeDouble", "Facade Double",
                     &value, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_string(&value, "facade string id",
                          strlen("facade string id"));
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, string_node, "facadeStringNode",
                     "Facade String Node", &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_data_value(
                     peer->server, string_node, &data_value, name_buffer, 4,
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("facade string id") + 1);
  CPKT_C89_CHECK(data_value.has_value == 0);
  cpkt_opcua_value_byte_string(&value, cpkt_c89_facade_bytes_value,
                               sizeof(cpkt_c89_facade_bytes_value));
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, bytes_node, "facadeBytes", "Facade Bytes",
                     &value, &status) == CPKT_OPCUA_OK);
  array_values[0] = 510;
  array_values[1] = 511;
  array_values[2] = 512;
  cpkt_opcua_value_integer_array(&value, array_values, 3);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, array_node, "facadeArrayDimensions",
                     "Facade Array Dimensions", &value,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_array_dimensions(
                     peer->server, array_node, dimensions, 0, &dimension_count,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_array_dimensions(
                     peer->server, array_node, dimensions, 1, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(dimensions[0] == 3UL);
  CPKT_C89_CHECK(cpkt_opcua_server_read_integer_array_range(
                     peer->server, array_node, "1:2", array_values, 2,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(array_values[0] == 511);
  CPKT_C89_CHECK(array_values[1] == 512);
  array_values[0] = 610;
  array_values[1] = 611;
  cpkt_opcua_value_integer_array(&value, array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_write_index_range(peer->server, array_node,
                                                     "1:2", &value,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_integer_array(
                     peer->server, array_node, array_values, 3,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 3);
  CPKT_C89_CHECK(array_values[0] == 510);
  CPKT_C89_CHECK(array_values[1] == 610);
  CPKT_C89_CHECK(array_values[2] == 611);
  bool_array_values[0] = 1;
  bool_array_values[1] = 0;
  cpkt_opcua_value_boolean_array(&value, bool_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, bool_array_node, "facadeBooleanArray",
                     "Facade Boolean Array", &value, &status) == CPKT_OPCUA_OK);
  bool_array_values[0] = 0;
  bool_array_values[1] = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_read_boolean_array(
                     peer->server, bool_array_node, bool_array_values, 2,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(bool_array_values[0] == 1);
  CPKT_C89_CHECK(bool_array_values[1] == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_read_boolean_array_range(
                     peer->server, bool_array_node, "1", bool_array_values, 1,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(bool_array_values[0] == 0);
  double_array_values[0] = 7.25;
  double_array_values[1] = 8.5;
  cpkt_opcua_value_double_array(&value, double_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, double_array_node, "facadeDoubleArray",
                     "Facade Double Array", &value, &status) == CPKT_OPCUA_OK);
  double_array_values[0] = 0.0;
  double_array_values[1] = 0.0;
  CPKT_C89_CHECK(cpkt_opcua_server_read_double_array(
                     peer->server, double_array_node, double_array_values, 2,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(double_array_values[0] > 7.24 &&
                 double_array_values[0] < 7.26);
  CPKT_C89_CHECK(double_array_values[1] > 8.49 &&
                 double_array_values[1] < 8.51);
  CPKT_C89_CHECK(cpkt_opcua_server_read_double_array_range(
                     peer->server, double_array_node, "1", double_array_values,
                     1, &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(double_array_values[0] > 8.49 &&
                 double_array_values[0] < 8.51);
  string_array_values[0].data = "alpha";
  string_array_values[0].length = strlen("alpha");
  string_array_values[1].data = "beta";
  string_array_values[1].length = strlen("beta");
  cpkt_opcua_value_string_array(&value, string_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, string_array_node, "facadeStringArray",
                     "Facade String Array", &value, &status) == CPKT_OPCUA_OK);
  memset(&string_array_seen, 0, sizeof(string_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_string_array(
                     peer->server, string_array_node,
                     cpkt_c89_string_array_callback, &string_array_seen,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(string_array_seen.count == 2);
  CPKT_C89_CHECK(string_array_seen.matched == 2);
  memset(&string_array_seen, 0, sizeof(string_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_string_array_range(
                     peer->server, string_array_node, "0:0",
                     cpkt_c89_string_array_callback, &string_array_seen,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(string_array_seen.count == 1);
  CPKT_C89_CHECK(string_array_seen.matched == 1);
  byte_string_array_0[0] = 0x10U;
  byte_string_array_0[1] = 0x11U;
  byte_string_array_1[0] = 0x20U;
  byte_string_array_1[1] = 0x21U;
  byte_string_array_1[2] = 0x22U;
  byte_string_array_values[0].data = byte_string_array_0;
  byte_string_array_values[0].length = sizeof(byte_string_array_0);
  byte_string_array_values[1].data = byte_string_array_1;
  byte_string_array_values[1].length = sizeof(byte_string_array_1);
  cpkt_opcua_value_byte_string_array(&value, byte_string_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, bytes_array_node, "facadeByteStringArray",
                     "Facade Byte String Array", &value,
                     &status) == CPKT_OPCUA_OK);
  memset(&byte_string_array_seen, 0, sizeof(byte_string_array_seen));
  CPKT_C89_CHECK(
      cpkt_opcua_server_read_byte_string_array(
          peer->server, bytes_array_node, cpkt_c89_byte_string_array_callback,
          &byte_string_array_seen, &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(byte_string_array_seen.count == 2);
  CPKT_C89_CHECK(byte_string_array_seen.matched == 2);
  memset(&byte_string_array_seen, 0, sizeof(byte_string_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_byte_string_array_range(
                     peer->server, bytes_array_node, "0:0",
                     cpkt_c89_byte_string_array_callback,
                     &byte_string_array_seen, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(byte_string_array_seen.count == 1);
  CPKT_C89_CHECK(byte_string_array_seen.matched == 1);
  cpkt_opcua_value_guid(&value, cpkt_c89_facade_guid_node_id);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, guid_value_node, "facadeGuidValue",
                     "Facade GUID Value", &value, &status) == CPKT_OPCUA_OK);
  memcpy(guid_array_values[0].bytes, cpkt_c89_native_guid_node_id, 16);
  memcpy(guid_array_values[1].bytes, cpkt_c89_facade_guid_node_id, 16);
  cpkt_opcua_value_guid_array(&value, guid_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, guid_array_node, "facadeGuidArray",
                     "Facade GUID Array", &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_guid_array(
                     peer->server, guid_array_node, guid_array_values, 2,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(memcmp(guid_array_values[0].bytes,
                        cpkt_c89_native_guid_node_id, 16) == 0);
  CPKT_C89_CHECK(memcmp(guid_array_values[1].bytes,
                        cpkt_c89_facade_guid_node_id, 16) == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_read_guid_array_range(
                     peer->server, guid_array_node, "1", guid_array_values, 1,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(memcmp(guid_array_values[0].bytes,
                        cpkt_c89_facade_guid_node_id, 16) == 0);
  cpkt_opcua_value_status(&value, 0);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, status_value_node, "facadeStatusValue",
                     "Facade Status Value", &value, &status) == CPKT_OPCUA_OK);
  status_array_values[0] = 0;
  status_array_values[1] = 0x803e0000UL;
  cpkt_opcua_value_status_array(&value, status_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, status_array_node, "facadeStatusArray",
                     "Facade Status Array", &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_status_array(
                     peer->server, status_array_node, status_array_values, 2,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(status_array_values[0] == 0);
  CPKT_C89_CHECK(status_array_values[1] == 0x803e0000UL);
  CPKT_C89_CHECK(cpkt_opcua_server_read_status_array_range(
                     peer->server, status_array_node, "1", status_array_values,
                     1, &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(status_array_values[0] == 0x803e0000UL);
  cpkt_opcua_value_uint64(&value, 0xffffffffUL, 0xffffffffUL);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, uint64_value_node, "facadeUInt64Value",
                     "Facade UInt64 Value", &value, &status) == CPKT_OPCUA_OK);
  uint64_array_values[0].high32 = 0x11111111UL;
  uint64_array_values[0].low32 = 0x22222222UL;
  uint64_array_values[1].high32 = 0x33333333UL;
  uint64_array_values[1].low32 = 0x44444444UL;
  cpkt_opcua_value_uint64_array(&value, uint64_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, uint64_array_node, "facadeUInt64Array",
                     "Facade UInt64 Array", &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_uint64_array(
                     peer->server, uint64_array_node, uint64_array_values, 2,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(uint64_array_values[0].high32 == 0x11111111UL);
  CPKT_C89_CHECK(uint64_array_values[0].low32 == 0x22222222UL);
  CPKT_C89_CHECK(uint64_array_values[1].high32 == 0x33333333UL);
  CPKT_C89_CHECK(uint64_array_values[1].low32 == 0x44444444UL);
  CPKT_C89_CHECK(cpkt_opcua_server_read_uint64_array_range(
                     peer->server, uint64_array_node, "1", uint64_array_values,
                     1, &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(uint64_array_values[0].high32 == 0x33333333UL);
  CPKT_C89_CHECK(uint64_array_values[0].low32 == 0x44444444UL);
  cpkt_opcua_value_datetime(&value, 0x01234567L, 0x76543210UL);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, datetime_value_node, "facadeDateTimeValue",
                     "Facade DateTime Value", &value,
                     &status) == CPKT_OPCUA_OK);
  datetime_array_values[0].high32 = 0x11111111L;
  datetime_array_values[0].low32 = 0x22222222UL;
  datetime_array_values[1].high32 = -2L;
  datetime_array_values[1].low32 = 0x33333333UL;
  datetime_array_values[2].high32 = -2147483647L - 1L;
  datetime_array_values[2].low32 = 0x44444444UL;
  cpkt_opcua_value_datetime_array(&value, datetime_array_values, 3);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, datetime_array_node, "facadeDateTimeArray",
                     "Facade DateTime Array", &value,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_datetime_array(
                     peer->server, datetime_array_node, datetime_array_values,
                     3, &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 3);
  CPKT_C89_CHECK(datetime_array_values[0].high32 == 0x11111111L);
  CPKT_C89_CHECK(datetime_array_values[0].low32 == 0x22222222UL);
  CPKT_C89_CHECK(datetime_array_values[1].high32 == -2L);
  CPKT_C89_CHECK(datetime_array_values[1].low32 == 0x33333333UL);
  CPKT_C89_CHECK(datetime_array_values[2].high32 == (-2147483647L - 1L));
  CPKT_C89_CHECK(datetime_array_values[2].low32 == 0x44444444UL);
  CPKT_C89_CHECK(cpkt_opcua_server_read_datetime_array_range(
                     peer->server, datetime_array_node, "1",
                     datetime_array_values, 1, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(datetime_array_values[0].high32 == -2L);
  CPKT_C89_CHECK(datetime_array_values[0].low32 == 0x33333333UL);
  cpkt_opcua_value_qualified_name(&value, CPKT_OPCUA_TEST_NS, "facadeQualified",
                                  strlen("facadeQualified"));
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, qualified_name_value_node,
                     "facadeQualifiedNameValue", "Facade Qualified Name Value",
                     &value, &status) == CPKT_OPCUA_OK);
  qualified_name_array_values[0].namespace_index = CPKT_OPCUA_TEST_NS;
  qualified_name_array_values[0].name = "alphaName";
  qualified_name_array_values[0].name_length = strlen("alphaName");
  qualified_name_array_values[1].namespace_index = CPKT_OPCUA_TEST_NS;
  qualified_name_array_values[1].name = "betaName";
  qualified_name_array_values[1].name_length = strlen("betaName");
  cpkt_opcua_value_qualified_name_array(&value, qualified_name_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, qualified_name_array_node,
                     "facadeQualifiedNameArray", "Facade Qualified Name Array",
                     &value, &status) == CPKT_OPCUA_OK);
  memset(&qualified_name_array_seen, 0, sizeof(qualified_name_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_qualified_name_array(
                     peer->server, qualified_name_array_node,
                     cpkt_c89_qualified_name_array_callback,
                     &qualified_name_array_seen, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(qualified_name_array_seen.count == 2);
  CPKT_C89_CHECK(qualified_name_array_seen.matched == 2);
  memset(&qualified_name_array_seen, 0, sizeof(qualified_name_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_qualified_name_array_range(
                     peer->server, qualified_name_array_node, "1",
                     cpkt_c89_qualified_name_array_callback,
                     &qualified_name_array_seen, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(qualified_name_array_seen.count == 1);
  CPKT_C89_CHECK(qualified_name_array_seen.matched == 1);
  cpkt_opcua_value_localized_text(&value, "en-US", strlen("en-US"),
                                  "facade text", strlen("facade text"));
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, localized_text_value_node,
                     "facadeLocalizedTextValue", "Facade Localized Text Value",
                     &value, &status) == CPKT_OPCUA_OK);
  localized_text_array_values[0].locale = "en-US";
  localized_text_array_values[0].locale_length = strlen("en-US");
  localized_text_array_values[0].text = "alpha text";
  localized_text_array_values[0].text_length = strlen("alpha text");
  localized_text_array_values[1].locale = "sv-SE";
  localized_text_array_values[1].locale_length = strlen("sv-SE");
  localized_text_array_values[1].text = "beta text";
  localized_text_array_values[1].text_length = strlen("beta text");
  cpkt_opcua_value_localized_text_array(&value, localized_text_array_values, 2);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, localized_text_array_node,
                     "facadeLocalizedTextArray", "Facade Localized Text Array",
                     &value, &status) == CPKT_OPCUA_OK);
  memset(&localized_text_array_seen, 0, sizeof(localized_text_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_localized_text_array(
                     peer->server, localized_text_array_node,
                     cpkt_c89_localized_text_array_callback,
                     &localized_text_array_seen, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 2);
  CPKT_C89_CHECK(localized_text_array_seen.count == 2);
  CPKT_C89_CHECK(localized_text_array_seen.matched == 2);
  memset(&localized_text_array_seen, 0, sizeof(localized_text_array_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_read_localized_text_array_range(
                     peer->server, localized_text_array_node, "1",
                     cpkt_c89_localized_text_array_callback,
                     &localized_text_array_seen, &dimension_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(localized_text_array_seen.count == 1);
  CPKT_C89_CHECK(localized_text_array_seen.matched == 1);
  cpkt_opcua_value_integer(&value, 414);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, guid_node, "facadeGuidNode",
                     "Facade GUID Node", &value, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_integer(&value, 415);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable(
                     peer->server, byte_node, "facadeByteNode",
                     "Facade Byte Node", &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_object(peer->server, object_node,
                                              cpkt_opcua_node_id_numeric(0, 85),
                                              "facadeObject", "Facade Object",
                                              &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_object(
                     peer->server, deleted_object_node,
                     cpkt_opcua_node_id_numeric(0, 85), "facadeDeletedObject",
                     "Facade Deleted Object", &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_delete_node(peer->server,
                                               deleted_object_node, 1,
                                               &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_class(
                     peer->server, deleted_object_node, &node_class, &status) ==
                 CPKT_OPCUA_ERR_UPSTREAM);
  cpkt_opcua_value_integer(&value, 11);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable_under(
                     peer->server, child_node, object_node, "facadeObjectChild",
                     "Facade Object Child", &value, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_class(peer->server, object_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_OBJECT);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_class(peer->server, child_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VARIABLE);
  CPKT_C89_CHECK(cpkt_opcua_server_add_object_type(
                     peer->server, object_type_node,
                     cpkt_opcua_node_id_numeric(0, 58), "facadeObjectType",
                     "Facade Object Type", 1, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_integer(&value, 93);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable_type(
                     peer->server, variable_type_node,
                     cpkt_opcua_node_id_numeric(0, 62), "facadeVariableType",
                     "Facade Variable Type", &value, 0,
                     &status) == CPKT_OPCUA_OK);
  array_values[0] = 7;
  array_values[1] = 8;
  array_values[2] = 9;
  cpkt_opcua_value_integer_array(&value, array_values, 3);
  CPKT_C89_CHECK(cpkt_opcua_server_add_variable_type(
                     peer->server, array_variable_type_node,
                     cpkt_opcua_node_id_numeric(0, 62),
                     "facadeArrayVariableType", "Facade Array Variable Type",
                     &value, 0, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_reference_type(
                     peer->server, reference_type_node,
                     cpkt_opcua_node_id_numeric(0, 31), "facadeReferenceType",
                     "Facade Reference Type", "Facade Reference Type Inverse",
                     0, 0, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_data_type(
                     peer->server, data_type_node,
                     cpkt_opcua_node_id_numeric(0, 24), "facadeDataType",
                     "Facade Data Type", 1, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_view(peer->server, view_node,
                                            cpkt_opcua_node_id_numeric(0, 87),
                                            "facadeView", "Facade View", 1, 0UL,
                                            &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(
      cpkt_opcua_server_read_node_class(peer->server, object_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_OBJECT_TYPE);
  CPKT_C89_CHECK(
      cpkt_opcua_server_read_node_class(peer->server, variable_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VARIABLE_TYPE);
  CPKT_C89_CHECK(
      cpkt_opcua_server_read_node_class(peer->server, array_variable_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VARIABLE_TYPE);
  CPKT_C89_CHECK(
      cpkt_opcua_server_read_value_rank(peer->server, array_variable_type_node,
                                        &value_rank, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(value_rank == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_array_dimensions(
                     peer->server, array_variable_type_node, dimensions, 1,
                     &dimension_count, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(dimension_count == 1);
  CPKT_C89_CHECK(dimensions[0] == 3UL);
  CPKT_C89_CHECK(
      cpkt_opcua_server_read_node_class(peer->server, reference_type_node,
                                        &node_class, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_REFERENCE_TYPE);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_class(peer->server, data_type_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_DATA_TYPE);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_class(peer->server, view_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_VIEW);
  CPKT_C89_CHECK(cpkt_opcua_server_read_is_abstract(
                     peer->server, object_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_is_abstract(
                     peer->server, data_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_inverse_name(
                     peer->server, reference_type_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Facade Reference Type Inverse") == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_read_symmetric(
                     peer->server, reference_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_symmetric(peer->server,
                                                   reference_type_node, 1,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_symmetric(
                     peer->server, reference_type_node, &boolean_attribute,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_write_symmetric(peer->server,
                                                   reference_type_node, 0,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_contains_no_loops(
                     peer->server, view_node, &boolean_attribute, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_write_contains_no_loops(
                     peer->server, view_node, 0, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_contains_no_loops(
                     peer->server, view_node, &boolean_attribute, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(boolean_attribute == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_contains_no_loops(
                     peer->server, view_node, 1, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_event_notifier(
                     peer->server, view_node, &event_notifier, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(event_notifier == 0UL);
  CPKT_C89_CHECK(cpkt_opcua_server_read_data_type(peer->server, int_node,
                                                  &data_type_node,
                                                  &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(data_type_node.namespace_index == 0);
  CPKT_C89_CHECK(data_type_node.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(data_type_node.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(cpkt_opcua_server_read_data_type(peer->server, bytes_node,
                                                  &data_type_node,
                                                  &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(data_type_node.namespace_index == 0);
  CPKT_C89_CHECK(data_type_node.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(data_type_node.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_BYTE_STRING);
  CPKT_C89_CHECK(cpkt_opcua_server_read_value_rank(peer->server, int_node,
                                                   &value_rank,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_access_level(peer->server, int_node,
                                                     &access_level,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_server_read_user_access_level(
                     peer->server, int_node, &access_level, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_server_read_write_mask(peer->server, int_node,
                                                   &write_mask,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((write_mask & 1UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_server_read_user_write_mask(peer->server, int_node,
                                                        &write_mask, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK((write_mask & 1UL) != 0);
  original_write_mask = write_mask;
  CPKT_C89_CHECK(cpkt_opcua_server_write_write_mask(peer->server, int_node, 1UL,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_write_mask(peer->server, int_node,
                                                   &write_mask,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(write_mask == 1UL);
  CPKT_C89_CHECK(cpkt_opcua_server_write_write_mask(peer->server, int_node,
                                                    original_write_mask,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(
      cpkt_opcua_server_write_data_type(
          peer->server, int_node,
          cpkt_opcua_node_id_numeric(0, CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER),
          &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_write_value_rank(peer->server, int_node, -1,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_access_level_ex(
                     peer->server, int_node, &access_level, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) != 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_access_level_ex(
                     peer->server, int_node, 1UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_access_level_ex(
                     peer->server, int_node, &access_level, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_access_level_ex(
                     peer->server, int_node, 3UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_minimum_sampling_interval(
                     peer->server, int_node, &minimum_sampling_interval,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(minimum_sampling_interval >= 0.0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_minimum_sampling_interval(
                     peer->server, int_node, 8.5, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_minimum_sampling_interval(
                     peer->server, int_node, &minimum_sampling_interval,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(minimum_sampling_interval > 8.4 &&
                 minimum_sampling_interval < 8.6);
  CPKT_C89_CHECK(cpkt_opcua_server_write_access_level(
                     peer->server, int_node, 1UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_access_level(peer->server, int_node,
                                                     &access_level,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK((access_level & 1UL) != 0);
  CPKT_C89_CHECK((access_level & 2UL) == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_access_level(
                     peer->server, int_node, 3UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_historizing(peer->server, int_node,
                                                    &historizing,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(historizing == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_historizing(peer->server, int_node, 1,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_historizing(peer->server, int_node,
                                                    &historizing,
                                                    &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(historizing == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_write_historizing(peer->server, int_node, 0,
                                                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_browse_name(
                     peer->server, object_node, &browse_namespace_index,
                     name_buffer, sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_namespace_index == CPKT_OPCUA_TEST_NS);
  CPKT_C89_CHECK(strcmp(name_buffer, "facadeObject") == 0);
  CPKT_C89_CHECK(required == strlen("facadeObject") + 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_display_name(
                     peer->server, object_node, name_buffer, 4, &required,
                     &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen("Facade Object") + 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_display_name(
                     peer->server, object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Facade Object") == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_display_name(
                     peer->server, object_node, "Facade Object Updated",
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_display_name(
                     peer->server, object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Facade Object Updated") == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_description(
                     peer->server, object_node, "Facade object description",
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_event_notifier(
                     peer->server, object_node, &event_notifier, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(event_notifier == 0UL);
  CPKT_C89_CHECK(cpkt_opcua_server_write_event_notifier(
                     peer->server, object_node, 1UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_event_notifier(
                     peer->server, object_node, &event_notifier, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(event_notifier == 1UL);
  CPKT_C89_CHECK(cpkt_opcua_server_write_event_notifier(
                     peer->server, object_node, 0UL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_description(
                     peer->server, object_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "Facade object description") == 0);
  object_child_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  object_child_path[0].browse_name = "facadeObject";
  object_child_path[1].namespace_index = CPKT_OPCUA_TEST_NS;
  object_child_path[1].browse_name = "facadeObjectChild";
  CPKT_C89_CHECK(cpkt_opcua_server_translate_browse_path(
                     peer->server, cpkt_opcua_node_id_numeric(0, 85),
                     object_child_path, 2, &translated_node, NULL, 0, &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_node_id_equal(translated_node, child_node) == 1);
  string_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  string_path[0].browse_name = "facadeStringNode";
  CPKT_C89_CHECK(cpkt_opcua_server_translate_browse_path(
                     peer->server, cpkt_opcua_node_id_numeric(0, 85),
                     string_path, 1, &translated_node, name_buffer, 4,
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen(CPKT_OPCUA_FACADE_STRING_NODE_ID) + 1);
  CPKT_C89_CHECK(cpkt_opcua_server_translate_browse_path(
                     peer->server, cpkt_opcua_node_id_numeric(0, 85),
                     string_path, 1, &translated_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_node_id_equal(translated_node, string_node) == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_id(
                     peer->server, object_node, &translated_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_node_id_equal(translated_node, object_node) == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_id(
                     peer->server, string_node, &translated_node, name_buffer,
                     4, &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required == strlen(CPKT_OPCUA_FACADE_STRING_NODE_ID) + 1);
  CPKT_C89_CHECK(translated_node.identifier_type == CPKT_OPCUA_NODE_ID_NULL);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_id(
                     peer->server, string_node, &translated_node, name_buffer,
                     sizeof(name_buffer), &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(translated_node.identifier_type == CPKT_OPCUA_NODE_ID_STRING);
  CPKT_C89_CHECK(translated_node.namespace_index == CPKT_OPCUA_TEST_NS);
  CPKT_C89_CHECK(
      strcmp(translated_node.string, CPKT_OPCUA_FACADE_STRING_NODE_ID) == 0);
  missing_path[0].namespace_index = CPKT_OPCUA_TEST_NS;
  missing_path[0].browse_name = "missingFacadeChild";
  CPKT_C89_CHECK(cpkt_opcua_server_translate_browse_path(
                     peer->server, cpkt_opcua_node_id_numeric(0, 85),
                     missing_path, 1, &translated_node, name_buffer,
                     sizeof(name_buffer), &required,
                     &status) == CPKT_OPCUA_ERR_UPSTREAM);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_children_ex(
                     peer->server, object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.child_seen == 1);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_OBJECT;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_children_ex(
                     peer->server, object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.child_seen == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_add_reference_ex(
                     peer->server, object_node,
                     cpkt_opcua_node_id_numeric(0, 47), 1,
                     cpkt_opcua_expanded_node_id_local(int_node),
                     CPKT_OPCUA_NODE_CLASS_VARIABLE, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.has_reference_type = 1;
  browse_options.reference_type_id = cpkt_opcua_node_id_numeric(0, 47);
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_children_ex(
                     peer->server, object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.int_seen == 1);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.max_references = 1;
  browse_options.node_class_mask = CPKT_OPCUA_NODE_CLASS_VARIABLE;
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_children_page(
                     peer->server, object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     small_continuation_point, sizeof(small_continuation_point),
                     &required, &status) == CPKT_OPCUA_ERR_RANGE);
  CPKT_C89_CHECK(required > sizeof(small_continuation_point));
  CPKT_C89_CHECK(browse_seen.child_seen == 0);
  CPKT_C89_CHECK(browse_seen.int_seen == 0);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_children_page(
                     peer->server, object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen, continuation_point,
                     sizeof(continuation_point), &required,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(required > 0);
  CPKT_C89_CHECK(browse_seen.child_seen + browse_seen.int_seen == 1);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_next(
                     peer->server, continuation_point, required, 0,
                     cpkt_c89_browse_callback, &browse_seen,
                     next_continuation_point, sizeof(next_continuation_point),
                     &next_required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(next_required == 0);
  CPKT_C89_CHECK(browse_seen.child_seen + browse_seen.int_seen == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_delete_reference_ex(
                     peer->server, object_node,
                     cpkt_opcua_node_id_numeric(0, 47), 1,
                     cpkt_opcua_expanded_node_id_local(int_node), 1,
                     &status) == CPKT_OPCUA_OK);
  memset(&browse_seen, 0, sizeof(browse_seen));
  CPKT_C89_CHECK(cpkt_opcua_server_browse_children_ex(
                     peer->server, object_node, &browse_options,
                     cpkt_c89_browse_callback, &browse_seen,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(browse_seen.int_seen == 0);
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  CPKT_C89_CHECK(cpkt_opcua_server_add_method(
                     peer->server, method_node, object_node, "facadeMultiply",
                     "Facade Multiply", method_input_types, 1,
                     CPKT_OPCUA_VALUE_INTEGER, cpkt_c89_multiply_method,
                     &method_factor, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_node_class(peer->server, method_node,
                                                   &node_class,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(node_class == CPKT_OPCUA_NODE_CLASS_METHOD);
  CPKT_C89_CHECK(cpkt_opcua_server_read_executable(peer->server, method_node,
                                                   &executable,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_user_executable(
                     peer->server, method_node, &executable, &status) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_write_executable(
                     peer->server, method_node, 0, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_executable(peer->server, method_node,
                                                   &executable,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(executable == 0);
  CPKT_C89_CHECK(cpkt_opcua_server_write_executable(
                     peer->server, method_node, 1, &status) == CPKT_OPCUA_OK);
  string_method_input_types[0] = CPKT_OPCUA_VALUE_STRING;
  CPKT_C89_CHECK(cpkt_opcua_server_add_method(
                     peer->server, echo_method_node, object_node, "facadeEcho",
                     "Facade Echo", string_method_input_types, 1,
                     CPKT_OPCUA_VALUE_STRING, cpkt_c89_echo_string_method, NULL,
                     &status) == CPKT_OPCUA_OK);
  bytes_method_input_types[0] = CPKT_OPCUA_VALUE_BYTE_STRING;
  CPKT_C89_CHECK(
      cpkt_opcua_server_add_method(
          peer->server,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                     CPKT_OPCUA_FACADE_BYTES_METHOD_ID),
          object_node, "facadeBytesEcho", "Facade Bytes Echo",
          bytes_method_input_types, 1, CPKT_OPCUA_VALUE_BYTE_STRING,
          cpkt_c89_echo_bytes_method, NULL, &status) == CPKT_OPCUA_OK);
  qualified_name_method_input_types[0] = CPKT_OPCUA_VALUE_QUALIFIED_NAME;
  CPKT_C89_CHECK(
      cpkt_opcua_server_add_method(
          peer->server, qualified_name_method_node, object_node,
          "facadeQualifiedNameEcho", "Facade QualifiedName Echo",
          qualified_name_method_input_types, 1, CPKT_OPCUA_VALUE_QUALIFIED_NAME,
          cpkt_c89_echo_qualified_name_method, NULL, &status) == CPKT_OPCUA_OK);
  localized_text_method_input_types[0] = CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
  CPKT_C89_CHECK(
      cpkt_opcua_server_add_method(
          peer->server, localized_text_method_node, object_node,
          "facadeLocalizedTextEcho", "Facade LocalizedText Echo",
          localized_text_method_input_types, 1, CPKT_OPCUA_VALUE_LOCALIZED_TEXT,
          cpkt_c89_echo_localized_text_method, NULL, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_add_method(
                     peer->server, empty_method_node, object_node,
                     "facadeEmpty", "Facade Empty", NULL, 0,
                     CPKT_OPCUA_VALUE_EMPTY, cpkt_c89_empty_method, NULL,
                     &status) == CPKT_OPCUA_OK);
  multi_output_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  multi_output_types[1] = CPKT_OPCUA_VALUE_DOUBLE;
  CPKT_C89_CHECK(cpkt_opcua_server_add_method_many(
                     peer->server, multi_method_node, object_node,
                     "facadeMulti", "Facade Multi", method_input_types, 1,
                     multi_output_types, 2, cpkt_c89_multi_method, NULL,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_read_method_argument_count(
                     peer->server, method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_INPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_method_argument(
                     peer->server, method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_INPUT, 0, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "input1") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_method_argument_count(
                     peer->server, method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_method_argument(
                     peer->server, method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 0, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "output1") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_INTEGER);
  CPKT_C89_CHECK(argument_value_rank == -1);
  CPKT_C89_CHECK(cpkt_opcua_server_read_method_argument_count(
                     peer->server, multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, &argument_count,
                     &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(argument_count == 2);
  CPKT_C89_CHECK(cpkt_opcua_server_read_method_argument(
                     peer->server, multi_method_node,
                     CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT, 1, &argument_data_type,
                     &argument_value_rank, name_buffer, sizeof(name_buffer),
                     &required, &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(strcmp(name_buffer, "output2") == 0);
  CPKT_C89_CHECK(argument_data_type.namespace_index == 0);
  CPKT_C89_CHECK(argument_data_type.identifier_type ==
                 CPKT_OPCUA_NODE_ID_NUMERIC);
  CPKT_C89_CHECK(argument_data_type.numeric ==
                 CPKT_OPCUA_STANDARD_DATA_TYPE_DOUBLE);
  CPKT_C89_CHECK(argument_value_rank == -1);
  *out = peer;
  return 0;
}

int cpkt_opcua_c89_server_peer_start(cpkt_opcua_c89_server_peer *peer) {
  cpkt_opcua_status status;
  int native_seen;

  CPKT_C89_CHECK(peer != NULL);
  CPKT_C89_CHECK(cpkt_opcua_server_startup(peer->server, &status) ==
                 CPKT_OPCUA_OK);
  native_seen = 0;
  CPKT_C89_CHECK(
      cpkt_opcua_server_pubsub_native(peer->server, cpkt_c89_native_server_seen,
                                      &native_seen) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  native_seen = 0;
  CPKT_C89_CHECK(cpkt_opcua_server_history_native(
                     peer->server, cpkt_c89_native_server_seen, &native_seen) ==
                 CPKT_OPCUA_OK);
  CPKT_C89_CHECK(native_seen == 1);
  return 0;
}

int cpkt_opcua_c89_server_peer_iterate(cpkt_opcua_c89_server_peer *peer) {
  CPKT_C89_CHECK(peer != NULL);
  CPKT_C89_CHECK(cpkt_opcua_server_iterate(peer->server, 1, NULL) ==
                 CPKT_OPCUA_OK);
  return 0;
}

int cpkt_opcua_c89_server_peer_trigger_event(cpkt_opcua_c89_server_peer *peer) {
  cpkt_opcua_server_event *event;
  cpkt_opcua_value value;
  cpkt_opcua_status status;
  unsigned char event_id[64];
  size_t required;

  CPKT_C89_CHECK(peer != NULL);
  event = NULL;
  CPKT_C89_CHECK(cpkt_opcua_server_create_event(
                     cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS,
                                                CPKT_OPCUA_FACADE_OBJECT_ID),
                     cpkt_opcua_node_id_numeric(0, 2041), 654UL,
                     "facade server event", &event, &status) == CPKT_OPCUA_OK);
  cpkt_opcua_value_localized_text(&value, "en-US", strlen("en-US"),
                                  "facade server event",
                                  strlen("facade server event"));
  CPKT_C89_CHECK(cpkt_opcua_server_event_set_field(event, 0, "Message", &value,
                                                   &status) == CPKT_OPCUA_OK);
  CPKT_C89_CHECK(cpkt_opcua_server_event_trigger(peer->server, event, event_id,
                                                 sizeof(event_id), &required,
                                                 &status) == CPKT_OPCUA_OK);
  cpkt_opcua_server_event_free(event);
  CPKT_C89_CHECK(required != 0);
  return 0;
}

int cpkt_opcua_c89_server_peer_shutdown(cpkt_opcua_c89_server_peer *peer) {
  cpkt_opcua_status status;

  CPKT_C89_CHECK(peer != NULL);
  CPKT_C89_CHECK(cpkt_opcua_server_shutdown(peer->server, &status) ==
                 CPKT_OPCUA_OK);
  return 0;
}

void cpkt_opcua_c89_server_peer_free(cpkt_opcua_c89_server_peer *peer) {
  if (peer != NULL) {
    cpkt_opcua_server_free(peer->server);
    free(peer);
  }
}
