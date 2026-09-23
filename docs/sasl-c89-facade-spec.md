# Cyrus SASL C89 facade

`cpkt-sasl` is the C89 receiver-shell boundary for the bundled Cyrus SASL
2.1.28 client and server library. It is distributed separately from raw
`libsasl2` as static and shared libraries, `CpktSasl`, and `cpkt-sasl.pc`.
The static `libsasl2.a` includes the GSSAPI/GS2 mechanism implementation;
static consumers need no loadable SASL modules. The shared SDK ships the
GSSAPI/GS2 modules in `lib/sasl2`, and shared `libsasl2` finds that directory
relative to its own installed location. Downstream distributions must retain
that directory alongside the bundled shared libraries.

`cpkt_sasl_client_new()` and `cpkt_sasl_server_new()` return a receiver whose
methods operate only on that connection. Call `receiver->close(receiver)` once
all provider-owned output and interaction views are no longer needed. Returned
text and interaction views are borrowed from Cyrus SASL and remain valid only
until its documented next operation on that connection or receiver close.

The facade-owned `cpkt_sasl_callbacks` record replaces the upstream
heterogeneous callback table. It is copied at initialization or connection
creation, and the facade retains the copied callbacks for the matching global
SASL lifetime or receiver lifetime. Callback contexts remain caller-owned.
Callbacks must not close their receiver while Cyrus SASL is invoking them.
Global initialization accepts only process callbacks; callbacks that receive a
connection must be supplied at receiver creation so the facade can preserve
receiver identity and lifetime.

The `secret` callback returns a borrowed `cpkt_sasl_secret` containing password
bytes and their exact length. The facade copies those bytes into native storage
for the receiver and clears that storage on the next secret callback or receiver
close. Return a null secret with `CPKT_SASL_OK` to cancel. Callback contexts and
the borrowed public secret remain caller-owned.

`encode` and `decode` preserve Cyrus SASL's provider-owned output lifetime;
they do not materialize or concatenate an input stream. Input byte counts use
`unsigned long` and are rejected when they cannot be represented by the
upstream library's bounded unsigned length.

When `start` or `step` returns `CPKT_SASL_INTERACT`, the caller fills the
borrowed interaction records and calls that same operation again with the
same interaction pointer. The facade forwards the completed records to Cyrus
SASL; they remain invalid after the next operation or receiver close.
