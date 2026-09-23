# Lua C89 facade

The complete Lua 5.5.1 C API is available to strict C89 consumers through
`<cpkt/lua.h>` and the separate `cpkt_lua` library. Use
`find_package(CpktLua CONFIG REQUIRED)` with `cpkt::lua` or
`cpkt::lua_shared`, or use `pkg-config --static --cflags --libs cpkt-lua`.
The installed header is generated from the three pinned upstream public Lua
headers and covers all 156 declared Lua, auxiliary-library, and standard-library
functions, plus C89 equivalents of the public convenience macros.

Names follow a mechanical mapping: `lua_*` becomes `cpkt_lua_*`, `luaL_*`
becomes `cpkt_lua_l_*`, and `luaopen_*` becomes `cpkt_lua_open_*`. Function
hover comments in the generated header link to the matching Lua 5.5 C API
entry or standard-library section; use the manual for stack effects, valid
indices, errors, and lifetimes.
The facade preserves Lua behavior, including Lua errors that long-jump through
unprotected calls. It is not an automatic protected-call layer.

Lua's configured integer is 64-bit. `cpkt_lua_integer` and
`cpkt_lua_unsigned` carry its exact bits in two 32-bit `unsigned int` fields,
`high` and `low`. Construct values with `cpkt_lua_integer_make()` or
`cpkt_lua_unsigned_make()` and inspect words with the matching helpers. Do
not cast these records to a C scalar or treat `high` as a numeric magnitude;
signed negative values use two's-complement bits. The generated facade owns
all conversion to and from the native Lua integer type.

The caller owns a state returned by `cpkt_lua_newstate()` or
`cpkt_lua_l_newstate()` and closes it with `cpkt_lua_close()`. Lua stack values,
strings, userdata, threads, and auxiliary buffers follow Lua's normal
ownership and lifetime rules. In particular, pointers returned by stack
string accessors are Lua-owned views. Reader chunks and writer callback data
are borrowed for the callback invocation. Callback records and their user
contexts remain caller-owned. The C89 callback typedefs in the generated
header document their return conventions.

For an embedding interface with policy controls rather than direct stack
access, use `<cpkt/lua_runtime.h>` and `cpkt::lua_runtime`. That facade owns
its state, exposes memory and instruction limits, and offers module/search
path helpers. It does not provide arbitrary Lua values or stack operations.
