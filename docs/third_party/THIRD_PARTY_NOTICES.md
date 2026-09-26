# Third party notices

The entries below identify the pinned PDF and ODBC dependencies and the generated OPC UA schema model. The complete license text for each is included in `share/doc/c.pkt.systems/third_party/<name>/LICENSE` in every SDK archive.

| Component | Version | License | Source |
| --- | --- | --- | --- |
| libHaru | 2.4.6 | Zlib style license | https://github.com/libharu/libharu/tree/v2.4.6 |
| libpng | 1.6.58 | libpng License 2.0 | https://github.com/pnggroup/libpng/tree/v1.6.58 |
| zlib | 1.3.2 | zlib license | https://zlib.net/ |
| iODBC | 3.52.16 | BSD three-clause license | https://github.com/openlink/iODBC/releases/tag/v3.52.16 |

The generated `cpkt/pdf.h` and `src/pdf.c` facade reproduce libHaru public declarations and adapt their types for C89 callers. The libHaru copyright and license terms apply to those reproduced declarations. The generator is `tools/generate_pdf_facade.py`.

The generated `cpkt/opcua_types.h` declarations are derived from the OPC
Foundation schema distributed with open62541 1.5.8 (OPC Foundation MIT License
1.00). Its copyright and permission notice are shipped in
`third_party/opcua-schema/LICENSE`. Generation reuses open62541's MPL-2.0
schema parser and C generator in a disposable build directory. The upstream
MPL-2.0 license and sources remain available with the bundled open62541 files;
the project backend and conversion engine do not replace the upstream ABI.

Generated `cpkt/opcua_constants.h` includes the standard namespace-zero NodeId
and StatusCode catalogues. NodeIds are emitted by upstream's generator; the
result is verified against the bundled native catalogue. These standard
catalogues share the OPC Foundation schema provenance above.

Generated `cpkt/opcua_plugins.h` reproduces the public open62541 access-control
and history-database/backend, historizing-settings and numeric-range declarations
under MPL-2.0, retaining the basysKom GmbH and full upstream `types.h`
copyright notices. Generated service/server bindings also derive their
declarations from open62541 public headers. The original MPL license and
upstream source are shipped with open62541; the maintained generator backends
and conversion implementation ship in the c.pkt.systems source archive.
