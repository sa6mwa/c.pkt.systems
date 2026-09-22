#include <cpkt/nghttp2.h>

#include <assert.h>
#include <stddef.h>

static int nghttp2_test_send_calls;

static cpkt_nghttp2_ssize nghttp2_test_send(
    cpkt_nghttp2_session *session, const cpkt_nghttp2_u8 *data,
    size_t length, int flags, void *context) {
  (void)session;
  (void)data;
  (void)length;
  (void)flags;
  (void)context;
  ++nghttp2_test_send_calls;
  return CPKT_NGHTTP2_ERR_WOULDBLOCK;
}

int main(void) {
  cpkt_nghttp2_session_callbacks *callbacks;
  cpkt_nghttp2_session *session;
  cpkt_nghttp2_option *option;
  cpkt_nghttp2_settings_entry setting;
  cpkt_nghttp2_u64 rate;
  const cpkt_nghttp2_info *info;
  int status;

  callbacks = NULL;
  session = NULL;
  option = NULL;
  status = cpkt_nghttp2_session_callbacks_new(&callbacks);
  assert(status == 0);
  assert(callbacks != NULL);
  cpkt_nghttp2_session_callbacks_set_send_callback2(callbacks,
                                                     nghttp2_test_send);
  status = cpkt_nghttp2_option_new(&option);
  assert(status == 0);
  assert(option != NULL);
  rate.high = 1UL;
  rate.low = 0UL;
  cpkt_nghttp2_option_set_glitch_rate_limit(option, rate, rate);
  status = cpkt_nghttp2_session_client_new3(&session, callbacks, NULL, option,
                                            NULL);
  assert(status == 0);
  assert(session != NULL);
  setting.settings_id = CPKT_NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  setting.value = 1U;
  status = cpkt_nghttp2_submit_settings(session, CPKT_NGHTTP2_FLAG_NONE,
                                        &setting, 1);
  assert(status == 0);
  assert(cpkt_nghttp2_session_want_write(session) != 0);
  /* nghttp2 retains the queued frame and reports a successful send pass when
   * the transport callback says would-block. */
  assert(cpkt_nghttp2_session_send(session) == 0);
  assert(nghttp2_test_send_calls == 1);
  info = cpkt_nghttp2_version(0);
  assert(info != NULL);
  assert(info->version_num == CPKT_NGHTTP2_VERSION_NUM);
  cpkt_nghttp2_session_del(session);
  cpkt_nghttp2_option_del(option);
  cpkt_nghttp2_session_callbacks_del(callbacks);
  return 0;
}
