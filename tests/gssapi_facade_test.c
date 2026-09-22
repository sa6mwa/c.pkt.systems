#include <cpkt/gssapi.h>

int main(void) {
  cpkt_gss_oid_set *mechanisms;
  cpkt_gss_oid *oid;
  cpkt_gss_buffer oid_text;
  cpkt_gss_buffer encoded;
  cpkt_gss_status minor;
  cpkt_gss_status status;
  int present;

  mechanisms = 0;
  status = cpkt_gss_indicate_mechanisms(&minor, &mechanisms);
  if (cpkt_gss_status_is_error(status) || mechanisms == 0 ||
      cpkt_gss_oid_set_count(mechanisms) == 0) {
    return 1;
  }
  oid_text.length = 9;
  oid_text.value = (void *)"{ 1 2 3 }";
  oid = 0;
  status = cpkt_gss_oid_from_text(&minor, &oid_text, &oid);
  if (cpkt_gss_status_is_error(status) || oid == 0) {
    cpkt_gss_release_oid_set(&minor, &mechanisms);
    return 2;
  }
  encoded.length = 0;
  encoded.value = 0;
  status = cpkt_gss_oid_to_text(&minor, oid, &encoded);
  if (cpkt_gss_status_is_error(status) || encoded.value == 0 ||
      encoded.length == 0) {
    cpkt_gss_release_oid(&minor, &oid);
    cpkt_gss_release_oid_set(&minor, &mechanisms);
    return 3;
  }
  cpkt_gss_release_buffer(&minor, &encoded);
  present = 1;
  status = cpkt_gss_oid_set_contains(&minor, oid, mechanisms, &present);
  if (cpkt_gss_status_is_error(status)) {
    cpkt_gss_release_oid(&minor, &oid);
    cpkt_gss_release_oid_set(&minor, &mechanisms);
    return 4;
  }
  cpkt_gss_release_oid(&minor, &oid);
  cpkt_gss_release_oid_set(&minor, &mechanisms);
  return 0;
}
