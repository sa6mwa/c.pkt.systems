#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <curl/curl.h>
#include <libssh2.h>
#include <libxml/parser.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <nghttp2/nghttp2.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <zlib.h>

static void cpkt_dependency_symbols_are_linkable(void **state) {
  const char *curl_version;
  const char *ssh2_version;
  nghttp2_info *http2_info;
  SSL_CTX *ctx;
  xmlDocPtr xml_doc;
  lua_State *lua_state;
  unsigned long zlib_flags;

  (void)state;

  curl_version = curl_version_info(CURLVERSION_NOW)->version;
  assert_non_null(curl_version);

  ssh2_version = libssh2_version(0);
  assert_non_null(ssh2_version);

  http2_info = nghttp2_version(NGHTTP2_VERSION_NUM);
  assert_non_null(http2_info);
  assert_non_null(http2_info->version_str);
  assert_non_null(OpenSSL_version(OPENSSL_VERSION));

  ctx = SSL_CTX_new(TLS_client_method());
  assert_non_null(ctx);
  SSL_CTX_free(ctx);

  zlib_flags = zlibCompileFlags();
  (void)zlib_flags;

  xml_doc = xmlReadMemory("<root/>", 7, "memory.xml", NULL, 0);
  assert_non_null(xml_doc);
  xmlFreeDoc(xml_doc);
  xmlCleanupParser();

  lua_state = luaL_newstate();
  assert_non_null(lua_state);
  luaL_openlibs(lua_state);
  lua_close(lua_state);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(cpkt_dependency_symbols_are_linkable),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
