#include <cpkt/opcua_types.h>
int main() {
  cpkt_opcua_ReadRequest request;
  cpkt_opcua_ReadRequest_init(&request);
  return request.nodesToReadSize == 0 ? 0 : 1;
}
