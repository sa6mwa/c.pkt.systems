#include <cpkt/mqttc.h>

#include <assert.h>
#include <string.h>

int main(void)
{
  cpkt_mqtt_u8 subscribe_buffer[64];
  cpkt_mqtt_u8 unsubscribe_buffer[64];
  cpkt_mqtt_u8 fixed_header_buffer[4];
  struct cpkt_mqtt_fixed_header fixed_header;
  cpkt_mqtt_ssize result;

  fixed_header.control_type = CPKT_MQTT_CONTROL_PINGREQ;
  fixed_header.control_flags = 0U;
  fixed_header.remaining_length = 0U;
  result = cpkt_mqtt_pack_fixed_header(fixed_header_buffer,
                                       sizeof(fixed_header_buffer),
                                       &fixed_header);
  assert(result == 2);
  assert(fixed_header_buffer[0] == 0xc0U);
  assert(fixed_header_buffer[1] == 0U);
  result = cpkt_mqtt_pack_subscribe_request(subscribe_buffer,
                                             sizeof(subscribe_buffer), 7U,
                                             "one", 1U, "two", 2U, NULL);
  assert(result > 0);
  assert(subscribe_buffer[0] == 0x82U);
  result = cpkt_mqtt_pack_unsubscribe_request(unsubscribe_buffer,
                                               sizeof(unsubscribe_buffer), 8U,
                                               "one", "two", NULL);
  assert(result > 0);
  assert(unsubscribe_buffer[0] == 0xa2U);
  assert(strcmp(cpkt_mqtt_error_str(CPKT_MQTT_ERROR_NULLPTR),
                "MQTT_ERROR_NULLPTR") == 0);
  return 0;
}
