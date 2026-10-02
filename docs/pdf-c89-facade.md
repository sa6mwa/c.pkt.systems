# PDF and PNG SDK surface

The SDK ships libHaru 2.4.6, libpng 1.6.58, and their bundled zlib dependency as static and shared libraries on every release target. libpng's complete public headers (`png.h`, `pngconf.h`, and `pnglibconf.h`) and the upstream library are exposed directly. libHaru's complete 290-function public API is exposed to strict C89 callers through `include/cpkt/pdf.h` and `libcpkt_pdf`. The upstream `hpdf*.h` headers and `libhpdf` are also included for source modes that support them.

The facade renames `HPDF_` types and constants to `CPKT_PDF_`, and converts function names to `cpkt_pdf_` plus lowercase words. For example, `HPDF_Page_DrawImage` is `cpkt_pdf_page_draw_image`. The generated header is the authoritative function list. `CPKT_PDF_UINT64` contains `lo` and `hi` unsigned 32-bit words; `cpkt_pdf_embedded_file_set_size` converts them to libHaru's 64-bit argument. All by-value Haru structures are copied across the boundary so the facade does not alias distinct C structure types. Callbacks use the same C calling conventions and argument values as upstream on the shipped targets.

`cpkt_pdf_new()` and `cpkt_pdf_new_ex()` return documents owned by the caller;
release them with `cpkt_pdf_free()`. Pages, fonts, images, and other objects
created for a document are owned by that document. Do not retain them after
freeing or replacing its contents. Errors use libHaru status codes and the
document's error callback/inspection API.

PDF output through `cpkt_pdf_save_to_stream()` is **buffered in memory**.
`cpkt_pdf_read_from_stream()` reads chunks from that saved buffer and
`cpkt_pdf_reset_stream()` rewinds it. `cpkt_pdf_get_contents()` also
serializes a complete PDF into a temporary memory stream before copying into
the caller's buffer. These functions are unsuitable when the document must be
streamed end to end with bounded memory use. `cpkt_pdf_save_to_file()` writes
to a caller-selected path.

For CMake, use `find_package(CpktPdf CONFIG REQUIRED)` and link `cpkt::pdf` for static or `cpkt::pdf_shared` for shared. `find_package(CpktPng CONFIG REQUIRED)` provides `cpkt::png`, `cpkt::png_static`, and `cpkt::png_shared`. `find_package(CpktHaru CONFIG REQUIRED)` provides `cpkt::haru`, `cpkt::haru_static`, and `cpkt::haru_shared` for native-header consumers. Static imported targets include the transitive libpng, zlib, and platform math dependencies.

For pkg-config, use `cpkt-pdf`, `cpkt-png`, or `cpkt-haru`. Use `pkg-config --static --cflags --libs cpkt-pdf` for static consumers. The SDK includes complete licenses under `share/doc/c.pkt.systems/third_party/` and an aggregate `THIRD_PARTY_NOTICES.md`.

`tests/pdf_facade_test.c` is a strict C89 facade test: it writes a PNG with libpng, embeds it with the PDF facade, checks ownership callbacks and error recovery, exercises metadata and page lookup, and verifies buffered PDF reads through both static and shared links. It checks the facade boundary rather than libHaru's PDF implementation. The facade source and header are generated from the pinned libHaru public headers by `tools/generate_pdf_facade.py` and formatted with the repository's host-provided clang-format.
