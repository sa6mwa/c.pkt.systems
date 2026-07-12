#include <cpkt/opcua.h>

#include <stddef.h>
#include <string.h>

struct cpkt_fuzz_cursor {
  const unsigned char *data;
  size_t size;
  size_t offset;
};

static unsigned char cpkt_fuzz_u8(struct cpkt_fuzz_cursor *cursor) {
  if (cursor->offset >= cursor->size) {
    return 0;
  }
  return cursor->data[cursor->offset++];
}

static unsigned long cpkt_fuzz_ulong(struct cpkt_fuzz_cursor *cursor) {
  unsigned long value;
  size_t i;

  value = 0;
  for (i = 0; i < sizeof(value); ++i) {
    value = (value << 8) | (unsigned long)cpkt_fuzz_u8(cursor);
  }
  return value;
}

static long cpkt_fuzz_long(struct cpkt_fuzz_cursor *cursor) {
  return (long)cpkt_fuzz_ulong(cursor);
}

static size_t cpkt_fuzz_size(struct cpkt_fuzz_cursor *cursor, size_t limit) {
  if (limit == 0) {
    return 0;
  }
  return (size_t)(cpkt_fuzz_ulong(cursor) % (unsigned long)(limit + 1U));
}

static const char *cpkt_fuzz_c_string(struct cpkt_fuzz_cursor *cursor,
                                      char *buffer, size_t buffer_size) {
  size_t length;
  size_t i;

  if (buffer_size == 0) {
    return "";
  }
  length = cpkt_fuzz_size(cursor, buffer_size - 1U);
  for (i = 0; i < length; ++i) {
    unsigned char byte;

    byte = cpkt_fuzz_u8(cursor);
    buffer[i] = (char)((byte % 95U) + 32U);
    if (buffer[i] == '\0') {
      buffer[i] = 'x';
    }
  }
  buffer[length] = '\0';
  return buffer;
}

static void cpkt_fuzz_guid(struct cpkt_fuzz_cursor *cursor,
                           unsigned char guid[16]) {
  size_t i;

  for (i = 0; i < 16U; ++i) {
    guid[i] = cpkt_fuzz_u8(cursor);
  }
}

static cpkt_opcua_node_id cpkt_fuzz_node_id(struct cpkt_fuzz_cursor *cursor,
                                            char *string_buffer,
                                            size_t string_buffer_size,
                                            unsigned char *byte_buffer,
                                            size_t byte_buffer_size) {
  unsigned char guid[16];
  unsigned short namespace_index;
  size_t byte_count;
  size_t i;

  namespace_index = (unsigned short)cpkt_fuzz_ulong(cursor);
  switch (cpkt_fuzz_u8(cursor) % 5U) {
  case 0:
    return cpkt_opcua_node_id_null();
  case 1:
    return cpkt_opcua_node_id_numeric(namespace_index, cpkt_fuzz_ulong(cursor));
  case 2:
    return cpkt_opcua_node_id_string(
        namespace_index,
        cpkt_fuzz_c_string(cursor, string_buffer, string_buffer_size));
  case 3:
    cpkt_fuzz_guid(cursor, guid);
    return cpkt_opcua_node_id_guid(namespace_index, guid);
  default:
    byte_count = cpkt_fuzz_size(cursor, byte_buffer_size);
    for (i = 0; i < byte_count; ++i) {
      byte_buffer[i] = cpkt_fuzz_u8(cursor);
    }
    return cpkt_opcua_node_id_byte_string(namespace_index, byte_buffer,
                                          byte_count);
  }
}

static void cpkt_fuzz_node_id_roundtrip(struct cpkt_fuzz_cursor *cursor) {
  char id_text[192];
  char id_storage[96];
  char parsed_storage[128];
  unsigned char bytes[64];
  cpkt_opcua_node_id node_id;
  cpkt_opcua_node_id parsed;
  size_t required;

  node_id = cpkt_fuzz_node_id(cursor, id_storage, sizeof(id_storage), bytes,
                              sizeof(bytes));
  (void)cpkt_opcua_node_id_equal(node_id, node_id);
  if (cpkt_opcua_node_id_print(node_id, id_text, sizeof(id_text), &required) ==
      CPKT_OPCUA_OK) {
    (void)cpkt_opcua_node_id_parse(
        id_text, &parsed, parsed_storage,
        cpkt_fuzz_size(cursor, sizeof(parsed_storage)), &required);
  }
  (void)cpkt_opcua_node_id_parse(
      cpkt_fuzz_c_string(cursor, id_text, sizeof(id_text)), &parsed,
      parsed_storage, cpkt_fuzz_size(cursor, sizeof(parsed_storage)),
      &required);
}

static void
cpkt_fuzz_expanded_node_id_roundtrip(struct cpkt_fuzz_cursor *cursor) {
  char namespace_uri[80];
  char id_text[256];
  char node_storage[80];
  char parsed_storage[160];
  unsigned char bytes[32];
  cpkt_opcua_node_id node_id;
  cpkt_opcua_expanded_node_id expanded;
  cpkt_opcua_expanded_node_id parsed;
  size_t required;

  node_id = cpkt_fuzz_node_id(cursor, node_storage, sizeof(node_storage), bytes,
                              sizeof(bytes));
  switch (cpkt_fuzz_u8(cursor) % 4U) {
  case 0:
    expanded = cpkt_opcua_expanded_node_id_local(node_id);
    break;
  case 1:
    cpkt_fuzz_c_string(cursor, namespace_uri, sizeof(namespace_uri));
    expanded = cpkt_opcua_expanded_node_id_uri(namespace_uri,
                                               strlen(namespace_uri), node_id);
    break;
  case 2:
    expanded =
        cpkt_opcua_expanded_node_id_server(cpkt_fuzz_ulong(cursor), node_id);
    break;
  default:
    cpkt_fuzz_c_string(cursor, namespace_uri, sizeof(namespace_uri));
    expanded = cpkt_opcua_expanded_node_id_server_uri(
        cpkt_fuzz_ulong(cursor), namespace_uri, strlen(namespace_uri), node_id);
    break;
  }
  (void)cpkt_opcua_expanded_node_id_equal(expanded, expanded);
  if (cpkt_opcua_expanded_node_id_print(expanded, id_text, sizeof(id_text),
                                        &required) == CPKT_OPCUA_OK) {
    (void)cpkt_opcua_expanded_node_id_parse(
        id_text, &parsed, parsed_storage,
        cpkt_fuzz_size(cursor, sizeof(parsed_storage)), &required);
  }
  (void)cpkt_opcua_expanded_node_id_parse(
      cpkt_fuzz_c_string(cursor, id_text, sizeof(id_text)), &parsed,
      parsed_storage, cpkt_fuzz_size(cursor, sizeof(parsed_storage)),
      &required);
}

static void cpkt_fuzz_text_helpers(struct cpkt_fuzz_cursor *cursor) {
  unsigned char guid[16];
  unsigned char parsed_guid[16];
  char text[256];
  char locale[64];
  char name[96];
  char parsed[192];
  const char *locale_out;
  const char *text_out;
  size_t required;
  size_t name_length;
  size_t locale_length;
  size_t text_length;
  unsigned short namespace_index;

  cpkt_fuzz_guid(cursor, guid);
  if (cpkt_opcua_guid_print(guid, text, sizeof(text), &required) ==
      CPKT_OPCUA_OK) {
    (void)cpkt_opcua_guid_parse(text, parsed_guid);
  }
  (void)cpkt_opcua_guid_parse(cpkt_fuzz_c_string(cursor, text, sizeof(text)),
                              parsed_guid);

  cpkt_fuzz_c_string(cursor, name, sizeof(name));
  (void)cpkt_opcua_qualified_name_print(
      (unsigned short)cpkt_fuzz_ulong(cursor), name, strlen(name), text,
      cpkt_fuzz_size(cursor, sizeof(text)), &required);
  (void)cpkt_opcua_qualified_name_parse(text, &namespace_index, parsed,
                                        cpkt_fuzz_size(cursor, sizeof(parsed)),
                                        &name_length, &required);

  cpkt_fuzz_c_string(cursor, locale, sizeof(locale));
  cpkt_fuzz_c_string(cursor, name, sizeof(name));
  (void)cpkt_opcua_localized_text_print(
      locale, strlen(locale), name, strlen(name), text,
      cpkt_fuzz_size(cursor, sizeof(text)), &required);
  (void)cpkt_opcua_localized_text_parse(
      text, parsed, cpkt_fuzz_size(cursor, sizeof(parsed)), &locale_out,
      &locale_length, &text_out, &text_length, &required);
}

static void cpkt_fuzz_values(struct cpkt_fuzz_cursor *cursor) {
  cpkt_opcua_value value;
  cpkt_opcua_data_value data_value;
  cpkt_opcua_uint64 uint64_values[4];
  cpkt_opcua_datetime datetime_values[4];
  cpkt_opcua_status status_values[4];
  cpkt_opcua_guid guid_values[4];
  cpkt_opcua_string_view string_views[4];
  cpkt_opcua_byte_string_view byte_string_views[4];
  cpkt_opcua_qualified_name_view qualified_names[4];
  cpkt_opcua_localized_text_view localized_texts[4];
  int bools[4];
  long integers[4];
  double doubles[4];
  char strings[4][32];
  unsigned char bytes[4][16];
  size_t count;
  size_t i;
  size_t j;

  cpkt_opcua_value_clear(&value);
  switch (cpkt_fuzz_u8(cursor) % 23U) {
  case 0:
    cpkt_opcua_value_boolean(&value, (int)cpkt_fuzz_u8(cursor));
    break;
  case 1:
    cpkt_opcua_value_integer(&value, cpkt_fuzz_long(cursor));
    break;
  case 2:
    cpkt_opcua_value_double(&value, (double)cpkt_fuzz_long(cursor) / 7.0);
    break;
  case 3:
    cpkt_fuzz_c_string(cursor, strings[0], sizeof(strings[0]));
    cpkt_opcua_value_string(&value, strings[0], strlen(strings[0]));
    break;
  case 4:
    count = cpkt_fuzz_size(cursor, sizeof(bytes[0]));
    for (i = 0; i < count; ++i) {
      bytes[0][i] = cpkt_fuzz_u8(cursor);
    }
    cpkt_opcua_value_byte_string(&value, bytes[0], count);
    break;
  case 5:
    cpkt_fuzz_guid(cursor, value.guid_value);
    cpkt_opcua_value_guid(&value, value.guid_value);
    break;
  case 6:
    cpkt_opcua_value_status(&value, cpkt_fuzz_ulong(cursor));
    break;
  case 7:
    cpkt_fuzz_c_string(cursor, strings[0], sizeof(strings[0]));
    cpkt_opcua_value_qualified_name(&value,
                                    (unsigned short)cpkt_fuzz_ulong(cursor),
                                    strings[0], strlen(strings[0]));
    break;
  case 8:
    cpkt_fuzz_c_string(cursor, strings[0], sizeof(strings[0]));
    cpkt_fuzz_c_string(cursor, strings[1], sizeof(strings[1]));
    cpkt_opcua_value_localized_text(&value, strings[0], strlen(strings[0]),
                                    strings[1], strlen(strings[1]));
    break;
  case 9:
    cpkt_opcua_value_uint64(&value, cpkt_fuzz_ulong(cursor),
                            cpkt_fuzz_ulong(cursor));
    break;
  case 10:
    cpkt_opcua_value_datetime(&value, cpkt_fuzz_long(cursor),
                              cpkt_fuzz_ulong(cursor));
    break;
  case 11:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      bools[i] = (int)cpkt_fuzz_u8(cursor);
    }
    cpkt_opcua_value_boolean_array(&value, bools, count);
    break;
  case 12:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      integers[i] = cpkt_fuzz_long(cursor);
    }
    cpkt_opcua_value_integer_array(&value, integers, count);
    break;
  case 13:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      doubles[i] = (double)cpkt_fuzz_long(cursor) / 11.0;
    }
    cpkt_opcua_value_double_array(&value, doubles, count);
    break;
  case 14:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      cpkt_fuzz_c_string(cursor, strings[i], sizeof(strings[i]));
      string_views[i].data = strings[i];
      string_views[i].length = strlen(strings[i]);
    }
    cpkt_opcua_value_string_array(&value, string_views, count);
    break;
  case 15:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      byte_string_views[i].length = cpkt_fuzz_size(cursor, sizeof(bytes[i]));
      for (j = 0; j < byte_string_views[i].length; ++j) {
        bytes[i][j] = cpkt_fuzz_u8(cursor);
      }
      byte_string_views[i].data = bytes[i];
    }
    cpkt_opcua_value_byte_string_array(&value, byte_string_views, count);
    break;
  case 16:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      uint64_values[i].high32 = cpkt_fuzz_ulong(cursor);
      uint64_values[i].low32 = cpkt_fuzz_ulong(cursor);
    }
    cpkt_opcua_value_uint64_array(&value, uint64_values, count);
    break;
  case 17:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      datetime_values[i].high32 = cpkt_fuzz_long(cursor);
      datetime_values[i].low32 = cpkt_fuzz_ulong(cursor);
    }
    cpkt_opcua_value_datetime_array(&value, datetime_values, count);
    break;
  case 18:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      status_values[i] = cpkt_fuzz_ulong(cursor);
    }
    cpkt_opcua_value_status_array(&value, status_values, count);
    break;
  case 19:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      cpkt_fuzz_guid(cursor, guid_values[i].bytes);
    }
    cpkt_opcua_value_guid_array(&value, guid_values, count);
    break;
  case 20:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      cpkt_fuzz_c_string(cursor, strings[i], sizeof(strings[i]));
      qualified_names[i].namespace_index =
          (unsigned short)cpkt_fuzz_ulong(cursor);
      qualified_names[i].name = strings[i];
      qualified_names[i].name_length = strlen(strings[i]);
    }
    cpkt_opcua_value_qualified_name_array(&value, qualified_names, count);
    break;
  default:
    count = cpkt_fuzz_size(cursor, 4);
    for (i = 0; i < count; ++i) {
      cpkt_fuzz_c_string(cursor, strings[i], sizeof(strings[i]));
      localized_texts[i].locale = i == 0 ? "en" : "";
      localized_texts[i].locale_length = strlen(localized_texts[i].locale);
      localized_texts[i].text = strings[i];
      localized_texts[i].text_length = strlen(strings[i]);
    }
    cpkt_opcua_value_localized_text_array(&value, localized_texts, count);
    break;
  }
  cpkt_opcua_data_value_clear(&data_value);
  data_value.has_value = 1;
  data_value.value = value;
  data_value.has_status = 1;
  data_value.status = cpkt_fuzz_ulong(cursor);
  data_value.has_source_timestamp = 1;
  data_value.source_timestamp.high32 = cpkt_fuzz_long(cursor);
  data_value.source_timestamp.low32 = cpkt_fuzz_ulong(cursor);
  cpkt_opcua_data_value_clear(&data_value);
}

static void cpkt_fuzz_options(struct cpkt_fuzz_cursor *cursor) {
  cpkt_opcua_browse_options browse_options;
  cpkt_opcua_monitor_options monitor_options;
  cpkt_opcua_mqtt_connection_options mqtt_options;
  cpkt_opcua_pubsub_writer_group_options writer_group_options;
  cpkt_opcua_pubsub_data_set_writer_options data_set_writer_options;
  cpkt_opcua_pubsub_reader_group_options reader_group_options;
  cpkt_opcua_pubsub_data_set_reader_options data_set_reader_options;

  (void)cursor;
  cpkt_opcua_browse_options_default(&browse_options);
  cpkt_opcua_monitor_options_default(&monitor_options);
  cpkt_opcua_mqtt_connection_options_default(&mqtt_options);
  cpkt_opcua_pubsub_writer_group_options_default(&writer_group_options);
  cpkt_opcua_pubsub_data_set_writer_options_default(&data_set_writer_options);
  cpkt_opcua_pubsub_reader_group_options_default(&reader_group_options);
  cpkt_opcua_pubsub_data_set_reader_options_default(&data_set_reader_options);
}

int cpkt_fuzz_one_input(const unsigned char *data, size_t size) {
  struct cpkt_fuzz_cursor cursor;

  cursor.data = data;
  cursor.size = size;
  cursor.offset = 0;

  cpkt_fuzz_node_id_roundtrip(&cursor);
  cpkt_fuzz_expanded_node_id_roundtrip(&cursor);
  cpkt_fuzz_text_helpers(&cursor);
  cpkt_fuzz_values(&cursor);
  cpkt_fuzz_options(&cursor);
  (void)cpkt_opcua_result_string(
      (cpkt_opcua_result)(cpkt_fuzz_u8(&cursor) % 12U));
  return 0;
}
