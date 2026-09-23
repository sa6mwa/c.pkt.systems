# PDF and PNG SDK surface

The SDK ships libHaru 2.4.6, libpng 1.6.58, and their bundled zlib dependency as static and shared libraries on every release target. libpng's complete public headers (`png.h`, `pngconf.h`, and `pnglibconf.h`) and the upstream library are exposed directly. libHaru's complete 290-function public API is exposed to strict C89 callers through `include/cpkt/pdf.h` and `libcpkt_pdf`. The upstream `hpdf*.h` headers and `libhpdf` are also included for source modes that support them.

The facade renames `HPDF_` types and constants to `CPKT_PDF_`, and converts function names to `cpkt_pdf_` plus lowercase words. For example, `HPDF_Page_DrawImage` is `cpkt_pdf_page_draw_image`. The generated header is the authoritative function list. `CPKT_PDF_UINT64` contains `lo` and `hi` unsigned 32-bit words; `cpkt_pdf_embedded_file_set_size` converts them to libHaru's 64-bit argument. All by-value Haru structures are copied across the boundary so the facade does not alias distinct C structure types. Callbacks use the same C calling conventions and argument values as upstream on the shipped targets.

For CMake, use `find_package(CpktPdf CONFIG REQUIRED)` and link `cpkt::pdf` for static or `cpkt::pdf_shared` for shared. `find_package(CpktPng CONFIG REQUIRED)` provides `cpkt::png`, `cpkt::png_static`, and `cpkt::png_shared`. `find_package(CpktHaru CONFIG REQUIRED)` provides `cpkt::haru`, `cpkt::haru_static`, and `cpkt::haru_shared` for native-header consumers. Static imported targets include the transitive libpng, zlib, and platform math dependencies.

For pkg-config, use `cpkt-pdf`, `cpkt-png`, or `cpkt-haru`. Use `pkg-config --static --cflags --libs cpkt-pdf` for static consumers. The SDK includes complete licenses under `share/doc/c.pkt.systems/third_party/` and an aggregate `THIRD_PARTY_NOTICES.md`.

`tests/pdf_facade_test.c` is a strict C89 example: it writes a PNG with libpng, embeds it with the PDF facade, and verifies a PDF stream through both static and shared links. The facade source and header are generated from the pinned libHaru public headers by `tools/generate_pdf_facade.py` and formatted with the repository's host-provided clang-format.
