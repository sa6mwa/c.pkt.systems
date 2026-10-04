# C89 cmocka test facade

Core contains the original native `cmocka.h`, native static/shared libraries and
metadata, alongside `cpkt/cmocka.h` and `CpktCmocka`. Native cmocka retains its
upstream C99 contract and ABI. The generated facade keeps its supported runner,
fixture, assertion, mocking, allocation and diagnostic surface behind a private
C99 conversion bridge. Native assertions and queue machinery remain responsible
for failure semantics and diagnostics. Production libraries never link cmocka.
The native Apache-2.0 copyright/license also applies to reproduced declarations.

ISO C89 has no current-function expression. Retrieval/check macros therefore
require the function token explicitly:

```c
#include <cpkt/cmocka.h>
static unsigned long service(long parameter) {
  check_expected_int(service, parameter);
  function_called(service);
  return mock_uint(service);
}
```

Queueing retains the explicit function argument: `will_return_uint(service, 7)`
and `expect_int_value(service, parameter, -3)`. Scalar integer conveniences use
`long` and `unsigned long`; set arrays use those same element types, and floating
set arrays use `double`. Public headers use no stdint/stdbool types, GNU function
intrinsics, variadic macros, compound literals or designated initializers.

Every integral native operation also has a `cpkt_cmocka_*_words` entrypoint using
`CpktCmockaWords {high, low}`. Each word contributes its low 32 bits; signed values
use the exact 64-bit two's-complement bit pattern. These operations work on 32-bit
ARM and 64-bit targets without `long long`. For full-width queues, use
`cpkt_cmocka_value_words(cpkt_cmocka_words(high, low))`; returned tagged values
expose `.words`. Typed callbacks use `CpktCMockaValueData` through the native
callback adapter; native headers and names remain separately available.

CMake imports `cpkt::cmocka_facade_static` or `cpkt::cmocka_facade_shared` from
`CpktCmocka`; native variants are `cpkt::cmocka_static` and
`cpkt::cmocka_shared`. The upstream `cmocka::cmocka` shared target is preserved.
Use `cpkt-cmocka.pc` after explicit installed validation for pkg-config consumers.
The executable downstream tests cover strict C89 compilation, setup/teardown,
scalar and full-width queues/sets/callbacks, pointer/floating values, native
allocation checks, and intentional failures with source locations.
