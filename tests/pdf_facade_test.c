#include <cpkt/pdf.h>
#include <png.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

typedef struct png_buffer {
  unsigned char data[4096];
  size_t size;
  int overflow;
} png_buffer;

#define PDF_REQUIRE(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "PDF assertion failed at line %d: %s\n", __LINE__,       \
              #condition);                                                     \
      goto done;                                                               \
    }                                                                          \
  } while (0)

static void png_write_memory(png_structp png, png_bytep data, png_size_t size) {
  png_buffer *buffer = (png_buffer *)png_get_io_ptr(png);
  if (size > sizeof(buffer->data) - buffer->size) {
    buffer->overflow = 1;
    png_error(png, "output buffer full");
  }
  memcpy(buffer->data + buffer->size, data, size);
  buffer->size += size;
}

static void png_flush_memory(png_structp png) { (void)png; }

int main(void) {
  png_structp writer;
  png_infop info;
  png_byte pixel[3];
  png_buffer buffer;
  CPKT_PDF_Doc pdf;
  CPKT_PDF_Page page;
  CPKT_PDF_Image image;
  CPKT_PDF_Font font;
  CPKT_PDF_Point position;
  CPKT_PDF_UINT32 size;
  CPKT_PDF_BYTE bytes[8192];
  int ok = 0;

  memset(&buffer, 0, sizeof(buffer));
  if (strcmp(png_get_libpng_ver(NULL), PNG_LIBPNG_VER_STRING) != 0)
    return 1;
  writer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!writer)
    return 2;
  info = png_create_info_struct(writer);
  if (!info) {
    png_destroy_write_struct(&writer, NULL);
    return 3;
  }
  if (setjmp(png_jmpbuf(writer))) {
    png_destroy_write_struct(&writer, &info);
    return 4;
  }
  png_set_write_fn(writer, &buffer, png_write_memory, png_flush_memory);
  png_set_IHDR(writer, info, 1, 1, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  pixel[0] = 255;
  pixel[1] = 0;
  pixel[2] = 0;
  png_write_info(writer, info);
  png_write_row(writer, pixel);
  png_write_end(writer, info);
  png_destroy_write_struct(&writer, &info);
  if (buffer.overflow || buffer.size == 0)
    return 5;

  pdf = cpkt_pdf_new(NULL, NULL);
  if (!pdf)
    return 6;
  page = cpkt_pdf_add_page(pdf);
  image = cpkt_pdf_load_png_image_from_mem(pdf, buffer.data,
                                           (CPKT_PDF_UINT)buffer.size);
  font = cpkt_pdf_get_font(pdf, "Helvetica", NULL);
  PDF_REQUIRE(page && image && font);
  PDF_REQUIRE(cpkt_pdf_page_draw_image(page, image, 40, 40, 40, 40) ==
              CPKT_PDF_OK);
  PDF_REQUIRE(cpkt_pdf_page_begin_text(page) == CPKT_PDF_OK);
  PDF_REQUIRE(cpkt_pdf_page_set_font_and_size(page, font, 12) == CPKT_PDF_OK);
  PDF_REQUIRE(cpkt_pdf_page_text_out(page, 60, 120, "c.pkt.systems PDF") ==
              CPKT_PDF_OK);
  position = cpkt_pdf_page_get_current_text_pos(page);
  PDF_REQUIRE(position.x > 60 && position.y == 120);
  PDF_REQUIRE(cpkt_pdf_page_end_text(page) == CPKT_PDF_OK);
  PDF_REQUIRE(cpkt_pdf_save_to_stream(pdf) == CPKT_PDF_OK);
  PDF_REQUIRE(cpkt_pdf_reset_stream(pdf) == CPKT_PDF_OK);
  size = cpkt_pdf_get_stream_size(pdf);
  PDF_REQUIRE(size >= 100 && size <= sizeof(bytes));
  PDF_REQUIRE(cpkt_pdf_read_from_stream(pdf, bytes, &size) == CPKT_PDF_OK);
  PDF_REQUIRE(size >= 100 && memcmp(bytes, "%PDF-", 5) == 0);
  ok = 1;
done:
  if (!ok)
    fprintf(stderr, "PDF facade failed, Haru error %lu\n",
            cpkt_pdf_get_error(pdf));
  cpkt_pdf_free(pdf);
  return ok ? 0 : 7;
}
