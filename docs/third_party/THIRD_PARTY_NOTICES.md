# Third party notices

The entries below identify the pinned PDF and ODBC dependencies. The complete license text for each is included in `share/doc/c.pkt.systems/third_party/<name>/LICENSE` in every SDK archive.

| Component | Version | License | Source |
| --- | --- | --- | --- |
| libHaru | 2.4.6 | Zlib style license | https://github.com/libharu/libharu/tree/v2.4.6 |
| libpng | 1.6.58 | libpng License 2.0 | https://github.com/pnggroup/libpng/tree/v1.6.58 |
| zlib | 1.3.2 | zlib license | https://zlib.net/ |
| iODBC | 3.52.16 | BSD three-clause license | https://github.com/openlink/iODBC/releases/tag/v3.52.16 |

The generated `cpkt/pdf.h` and `src/pdf.c` facade reproduce libHaru public declarations and adapt their types for C89 callers. The libHaru copyright and license terms apply to those reproduced declarations. The generator is `tools/generate_pdf_facade.py`.
