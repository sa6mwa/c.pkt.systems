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
