/* Generated from libHaru 2.4.6 public headers; see
 * tools/generate_pdf_facade.py. */
#include "cpkt/pdf.h"
#define HPDF_SHARED 1
#include <hpdf.h>
#include <string.h>
typedef char cpkt_pdf_uint32_is_32_bits[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char cpkt_pdf_hpdf_3dmatrix_layout
    [(sizeof(HPDF_3DMatrix) == sizeof(CPKT_PDF_3DMatrix)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_box_layout[(sizeof(HPDF_Box) == sizeof(CPKT_PDF_Box))
                                          ? 1
                                          : -1];
typedef char cpkt_pdf_hpdf_cmykcolor_layout
    [(sizeof(HPDF_CMYKColor) == sizeof(CPKT_PDF_CMYKColor)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_dashmode_layout
    [(sizeof(HPDF_DashMode) == sizeof(CPKT_PDF_DashMode)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_date_layout
    [(sizeof(HPDF_Date) == sizeof(CPKT_PDF_Date)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_point_layout
    [(sizeof(HPDF_Point) == sizeof(CPKT_PDF_Point)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_point3d_layout
    [(sizeof(HPDF_Point3D) == sizeof(CPKT_PDF_Point3D)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_rgbcolor_layout
    [(sizeof(HPDF_RGBColor) == sizeof(CPKT_PDF_RGBColor)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_rect_layout
    [(sizeof(HPDF_Rect) == sizeof(CPKT_PDF_Rect)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_textwidth_layout
    [(sizeof(HPDF_TextWidth) == sizeof(CPKT_PDF_TextWidth)) ? 1 : -1];
typedef char cpkt_pdf_hpdf_transmatrix_layout
    [(sizeof(HPDF_TransMatrix) == sizeof(CPKT_PDF_TransMatrix)) ? 1 : -1];

/** Returns a static libHaru version string; do not free it. */
const char *cpkt_pdf_get_version(void) { return HPDF_GetVersion(); }

/** Creates an owned document with caller allocator and error callbacks; release
 * it with cpkt_pdf_free(). */
CPKT_PDF_Doc cpkt_pdf_new_ex(CPKT_PDF_Error_Handler user_error_fn,
                             CPKT_PDF_Alloc_Func user_alloc_fn,
                             CPKT_PDF_Free_Func user_free_fn,
                             CPKT_PDF_UINT mem_pool_buf_size, void *user_data) {
  return HPDF_NewEx(user_error_fn, user_alloc_fn, user_free_fn,
                    mem_pool_buf_size, user_data);
}

/** Creates an owned document; release it with cpkt_pdf_free(). */
CPKT_PDF_Doc cpkt_pdf_new(CPKT_PDF_Error_Handler user_error_fn,
                          void *user_data) {
  return HPDF_New(user_error_fn, user_data);
}

/** Calls libHaru's HPDF_SetErrorHandler with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_set_error_handler(CPKT_PDF_Doc pdf,
                           CPKT_PDF_Error_Handler user_error_fn) {
  return HPDF_SetErrorHandler(pdf, user_error_fn);
}

/** Releases the document and all objects owned by it. */
void cpkt_pdf_free(CPKT_PDF_Doc pdf) { HPDF_Free(pdf); }

/** Calls libHaru's HPDF_GetDocMMgr with C89 facade types. */
CPKT_PDF_MMgr cpkt_pdf_get_doc_m_mgr(CPKT_PDF_Doc doc) {
  return HPDF_GetDocMMgr(doc);
}

/** Starts a new document in this handle; previously created document objects
 * become invalid. */
CPKT_PDF_STATUS cpkt_pdf_new_doc(CPKT_PDF_Doc pdf) { return HPDF_NewDoc(pdf); }

/** Releases the current document and its page, font, and image objects. */
void cpkt_pdf_free_doc(CPKT_PDF_Doc pdf) { HPDF_FreeDoc(pdf); }

/** Calls libHaru's HPDF_HasDoc with C89 facade types. */
CPKT_PDF_BOOL cpkt_pdf_has_doc(CPKT_PDF_Doc pdf) { return HPDF_HasDoc(pdf); }

/** Releases every document owned by this handle. */
void cpkt_pdf_free_doc_all(CPKT_PDF_Doc pdf) { HPDF_FreeDocAll(pdf); }

/** Serializes the full PDF into libHaru's in-memory stream; this is buffered
 * output. */
CPKT_PDF_STATUS cpkt_pdf_save_to_stream(CPKT_PDF_Doc pdf) {
  return HPDF_SaveToStream(pdf);
}

/** Serializes a full PDF into temporary memory, then copies up to the input
 * size into buf and writes the copied byte count to size. */
CPKT_PDF_STATUS cpkt_pdf_get_contents(CPKT_PDF_Doc pdf, CPKT_PDF_BYTE *buf,
                                      CPKT_PDF_UINT32 *size) {
  return HPDF_GetContents(pdf, buf, size);
}

/** Returns the size of the document's saved in-memory stream. */
CPKT_PDF_UINT32 cpkt_pdf_get_stream_size(CPKT_PDF_Doc pdf) {
  return HPDF_GetStreamSize(pdf);
}

/** Reads up to the input size from the saved in-memory PDF stream and writes
 * the byte count read to size. */
CPKT_PDF_STATUS cpkt_pdf_read_from_stream(CPKT_PDF_Doc pdf, CPKT_PDF_BYTE *buf,
                                          CPKT_PDF_UINT32 *size) {
  return HPDF_ReadFromStream(pdf, buf, size);
}

/** Rewinds the saved in-memory stream for another read. */
CPKT_PDF_STATUS cpkt_pdf_reset_stream(CPKT_PDF_Doc pdf) {
  return HPDF_ResetStream(pdf);
}

/** Serializes the PDF to the named file; the caller owns the pathname. */
CPKT_PDF_STATUS cpkt_pdf_save_to_file(CPKT_PDF_Doc pdf, const char *file_name) {
  return HPDF_SaveToFile(pdf, file_name);
}

/** Calls libHaru's HPDF_GetError with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_get_error(CPKT_PDF_Doc pdf) {
  return HPDF_GetError(pdf);
}

/** Calls libHaru's HPDF_GetErrorDetail with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_get_error_detail(CPKT_PDF_Doc pdf) {
  return HPDF_GetErrorDetail(pdf);
}

/** Calls libHaru's HPDF_ResetError with C89 facade types. */
void cpkt_pdf_reset_error(CPKT_PDF_Doc pdf) { HPDF_ResetError(pdf); }

/** Calls libHaru's HPDF_CheckError with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_check_error(CPKT_PDF_Error error) {
  return HPDF_CheckError(error);
}

/** Calls libHaru's HPDF_SetPagesConfiguration with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_pages_configuration(CPKT_PDF_Doc pdf,
                                                 CPKT_PDF_UINT page_per_pages) {
  return HPDF_SetPagesConfiguration(pdf, page_per_pages);
}

/** Calls libHaru's HPDF_GetPageByIndex with C89 facade types. */
CPKT_PDF_Page cpkt_pdf_get_page_by_index(CPKT_PDF_Doc pdf,
                                         CPKT_PDF_UINT index) {
  return HPDF_GetPageByIndex(pdf, index);
}

/** Calls libHaru's HPDF_SetPDFAConformance with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_pdfa_conformance(CPKT_PDF_Doc pdf,
                                              CPKT_PDF_PDFAType pdfa_type) {
  return HPDF_SetPDFAConformance(pdf, (HPDF_PDFAType)pdfa_type);
}

/** Calls libHaru's HPDF_AddPDFAXmpExtension with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_add_pdfa_xmp_extension(CPKT_PDF_Doc pdf,
                                                const char *xmp_description) {
  return HPDF_AddPDFAXmpExtension(pdf, xmp_description);
}

/** Calls libHaru's HPDF_AppendOutputIntents with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_append_output_intents(CPKT_PDF_Doc pdf,
                                               const char *iccname,
                                               CPKT_PDF_Dict iccdict) {
  return HPDF_AppendOutputIntents(pdf, iccname, iccdict);
}

/** Calls libHaru's HPDF_GetPageMMgr with C89 facade types. */
CPKT_PDF_MMgr cpkt_pdf_get_page_m_mgr(CPKT_PDF_Page page) {
  return HPDF_GetPageMMgr(page);
}

/** Calls libHaru's HPDF_GetPageLayout with C89 facade types. */
CPKT_PDF_PageLayout cpkt_pdf_get_page_layout(CPKT_PDF_Doc pdf) {
  return (CPKT_PDF_PageLayout)HPDF_GetPageLayout(pdf);
}

/** Calls libHaru's HPDF_SetPageLayout with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_page_layout(CPKT_PDF_Doc pdf,
                                         CPKT_PDF_PageLayout layout) {
  return HPDF_SetPageLayout(pdf, (HPDF_PageLayout)layout);
}

/** Calls libHaru's HPDF_GetPageMode with C89 facade types. */
CPKT_PDF_PageMode cpkt_pdf_get_page_mode(CPKT_PDF_Doc pdf) {
  return (CPKT_PDF_PageMode)HPDF_GetPageMode(pdf);
}

/** Calls libHaru's HPDF_SetPageMode with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_page_mode(CPKT_PDF_Doc pdf,
                                       CPKT_PDF_PageMode mode) {
  return HPDF_SetPageMode(pdf, (HPDF_PageMode)mode);
}

/** Calls libHaru's HPDF_GetViewerPreference with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_get_viewer_preference(CPKT_PDF_Doc pdf) {
  return HPDF_GetViewerPreference(pdf);
}

/** Calls libHaru's HPDF_SetViewerPreference with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_viewer_preference(CPKT_PDF_Doc pdf,
                                               CPKT_PDF_UINT value) {
  return HPDF_SetViewerPreference(pdf, value);
}

/** Calls libHaru's HPDF_SetOpenAction with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_open_action(CPKT_PDF_Doc pdf,
                                         CPKT_PDF_Destination open_action) {
  return HPDF_SetOpenAction(pdf, open_action);
}

/** Calls libHaru's HPDF_GetCurrentPage with C89 facade types. */
CPKT_PDF_Page cpkt_pdf_get_current_page(CPKT_PDF_Doc pdf) {
  return HPDF_GetCurrentPage(pdf);
}

/** Adds a page owned by the current document; free the document to release it.
 */
CPKT_PDF_Page cpkt_pdf_add_page(CPKT_PDF_Doc pdf) { return HPDF_AddPage(pdf); }

/** Calls libHaru's HPDF_InsertPage with C89 facade types. */
CPKT_PDF_Page cpkt_pdf_insert_page(CPKT_PDF_Doc pdf, CPKT_PDF_Page page) {
  return HPDF_InsertPage(pdf, page);
}

/** Calls libHaru's HPDF_Page_SetWidth with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_width(CPKT_PDF_Page page,
                                        CPKT_PDF_REAL value) {
  return HPDF_Page_SetWidth(page, value);
}

/** Calls libHaru's HPDF_Page_SetHeight with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_height(CPKT_PDF_Page page,
                                         CPKT_PDF_REAL value) {
  return HPDF_Page_SetHeight(page, value);
}

/** Calls libHaru's HPDF_Page_SetBoundary with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_page_set_boundary(CPKT_PDF_Page page, CPKT_PDF_PageBoundary boundary,
                           CPKT_PDF_REAL left, CPKT_PDF_REAL bottom,
                           CPKT_PDF_REAL right, CPKT_PDF_REAL top) {
  return HPDF_Page_SetBoundary(page, (HPDF_PageBoundary)boundary, left, bottom,
                               right, top);
}

/** Calls libHaru's HPDF_Page_SetSize with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_size(CPKT_PDF_Page page,
                                       CPKT_PDF_PageSizes size,
                                       CPKT_PDF_PageDirection direction) {
  return HPDF_Page_SetSize(page, (HPDF_PageSizes)size,
                           (HPDF_PageDirection)direction);
}

/** Calls libHaru's HPDF_Page_SetRotate with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_rotate(CPKT_PDF_Page page,
                                         CPKT_PDF_UINT16 angle) {
  return HPDF_Page_SetRotate(page, angle);
}

/** Calls libHaru's HPDF_Page_SetZoom with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_zoom(CPKT_PDF_Page page, CPKT_PDF_REAL zoom) {
  return HPDF_Page_SetZoom(page, zoom);
}

/** Calls libHaru's HPDF_GetFont with C89 facade types. */
CPKT_PDF_Font cpkt_pdf_get_font(CPKT_PDF_Doc pdf, const char *font_name,
                                const char *encoding_name) {
  return HPDF_GetFont(pdf, font_name, encoding_name);
}

/** Calls libHaru's HPDF_LoadType1FontFromFile with C89 facade types. */
const char *cpkt_pdf_load_type1_font_from_file(CPKT_PDF_Doc pdf,
                                               const char *afm_file_name,
                                               const char *data_file_name) {
  return HPDF_LoadType1FontFromFile(pdf, afm_file_name, data_file_name);
}

/** Calls libHaru's HPDF_GetTTFontDefFromFile with C89 facade types. */
CPKT_PDF_FontDef cpkt_pdf_get_tt_font_def_from_file(CPKT_PDF_Doc pdf,
                                                    const char *file_name,
                                                    CPKT_PDF_BOOL embedding) {
  return HPDF_GetTTFontDefFromFile(pdf, file_name, embedding);
}

/** Calls libHaru's HPDF_LoadTTFontFromFile with C89 facade types. */
const char *cpkt_pdf_load_tt_font_from_file(CPKT_PDF_Doc pdf,
                                            const char *file_name,
                                            CPKT_PDF_BOOL embedding) {
  return HPDF_LoadTTFontFromFile(pdf, file_name, embedding);
}

/** Calls libHaru's HPDF_LoadTTFontFromFile2 with C89 facade types. */
const char *cpkt_pdf_load_tt_font_from_file2(CPKT_PDF_Doc pdf,
                                             const char *file_name,
                                             CPKT_PDF_UINT index,
                                             CPKT_PDF_BOOL embedding) {
  return HPDF_LoadTTFontFromFile2(pdf, file_name, index, embedding);
}

/** Calls libHaru's HPDF_LoadTTFontFromMemory with C89 facade types. */
const char *cpkt_pdf_load_tt_font_from_memory(CPKT_PDF_Doc pdf,
                                              const CPKT_PDF_BYTE *buffer,
                                              CPKT_PDF_UINT size,
                                              CPKT_PDF_BOOL embedding) {
  return HPDF_LoadTTFontFromMemory(pdf, buffer, size, embedding);
}

/** Calls libHaru's HPDF_AddPageLabel with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_add_page_label(CPKT_PDF_Doc pdf,
                                        CPKT_PDF_UINT page_num,
                                        CPKT_PDF_PageNumStyle style,
                                        CPKT_PDF_UINT first_page,
                                        const char *prefix) {
  return HPDF_AddPageLabel(pdf, page_num, (HPDF_PageNumStyle)style, first_page,
                           prefix);
}

/** Calls libHaru's HPDF_UseJPFonts with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_jp_fonts(CPKT_PDF_Doc pdf) {
  return HPDF_UseJPFonts(pdf);
}

/** Calls libHaru's HPDF_UseKRFonts with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_kr_fonts(CPKT_PDF_Doc pdf) {
  return HPDF_UseKRFonts(pdf);
}

/** Calls libHaru's HPDF_UseCNSFonts with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_cns_fonts(CPKT_PDF_Doc pdf) {
  return HPDF_UseCNSFonts(pdf);
}

/** Calls libHaru's HPDF_UseCNTFonts with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_cnt_fonts(CPKT_PDF_Doc pdf) {
  return HPDF_UseCNTFonts(pdf);
}

/** Calls libHaru's HPDF_CreateOutline with C89 facade types. */
CPKT_PDF_Outline cpkt_pdf_create_outline(CPKT_PDF_Doc pdf,
                                         CPKT_PDF_Outline parent,
                                         const char *title,
                                         CPKT_PDF_Encoder encoder) {
  return HPDF_CreateOutline(pdf, parent, title, encoder);
}

/** Calls libHaru's HPDF_Outline_SetOpened with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_outline_set_opened(CPKT_PDF_Outline outline,
                                            CPKT_PDF_BOOL opened) {
  return HPDF_Outline_SetOpened(outline, opened);
}

/** Calls libHaru's HPDF_Outline_SetDestination with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_outline_set_destination(CPKT_PDF_Outline outline,
                                                 CPKT_PDF_Destination dst) {
  return HPDF_Outline_SetDestination(outline, dst);
}

/** Calls libHaru's HPDF_Page_CreateDestination with C89 facade types. */
CPKT_PDF_Destination cpkt_pdf_page_create_destination(CPKT_PDF_Page page) {
  return HPDF_Page_CreateDestination(page);
}

/** Calls libHaru's HPDF_Destination_SetXYZ with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_xyz(CPKT_PDF_Destination dst,
                                             CPKT_PDF_REAL left,
                                             CPKT_PDF_REAL top,
                                             CPKT_PDF_REAL zoom) {
  return HPDF_Destination_SetXYZ(dst, left, top, zoom);
}

/** Calls libHaru's HPDF_Destination_SetFit with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit(CPKT_PDF_Destination dst) {
  return HPDF_Destination_SetFit(dst);
}

/** Calls libHaru's HPDF_Destination_SetFitH with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_h(CPKT_PDF_Destination dst,
                                               CPKT_PDF_REAL top) {
  return HPDF_Destination_SetFitH(dst, top);
}

/** Calls libHaru's HPDF_Destination_SetFitV with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_v(CPKT_PDF_Destination dst,
                                               CPKT_PDF_REAL left) {
  return HPDF_Destination_SetFitV(dst, left);
}

/** Calls libHaru's HPDF_Destination_SetFitR with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_r(CPKT_PDF_Destination dst,
                                               CPKT_PDF_REAL left,
                                               CPKT_PDF_REAL bottom,
                                               CPKT_PDF_REAL right,
                                               CPKT_PDF_REAL top) {
  return HPDF_Destination_SetFitR(dst, left, bottom, right, top);
}

/** Calls libHaru's HPDF_Destination_SetFitB with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_b(CPKT_PDF_Destination dst) {
  return HPDF_Destination_SetFitB(dst);
}

/** Calls libHaru's HPDF_Destination_SetFitBH with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_bh(CPKT_PDF_Destination dst,
                                                CPKT_PDF_REAL top) {
  return HPDF_Destination_SetFitBH(dst, top);
}

/** Calls libHaru's HPDF_Destination_SetFitBV with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_bv(CPKT_PDF_Destination dst,
                                                CPKT_PDF_REAL left) {
  return HPDF_Destination_SetFitBV(dst, left);
}

/** Calls libHaru's HPDF_GetEncoder with C89 facade types. */
CPKT_PDF_Encoder cpkt_pdf_get_encoder(CPKT_PDF_Doc pdf,
                                      const char *encoding_name) {
  return HPDF_GetEncoder(pdf, encoding_name);
}

/** Calls libHaru's HPDF_GetCurrentEncoder with C89 facade types. */
CPKT_PDF_Encoder cpkt_pdf_get_current_encoder(CPKT_PDF_Doc pdf) {
  return HPDF_GetCurrentEncoder(pdf);
}

/** Calls libHaru's HPDF_SetCurrentEncoder with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_current_encoder(CPKT_PDF_Doc pdf,
                                             const char *encoding_name) {
  return HPDF_SetCurrentEncoder(pdf, encoding_name);
}

/** Calls libHaru's HPDF_Encoder_GetType with C89 facade types. */
CPKT_PDF_EncoderType cpkt_pdf_encoder_get_type(CPKT_PDF_Encoder encoder) {
  return (CPKT_PDF_EncoderType)HPDF_Encoder_GetType(encoder);
}

/** Calls libHaru's HPDF_Encoder_GetByteType with C89 facade types. */
CPKT_PDF_ByteType cpkt_pdf_encoder_get_byte_type(CPKT_PDF_Encoder encoder,
                                                 const char *text,
                                                 CPKT_PDF_UINT index) {
  return (CPKT_PDF_ByteType)HPDF_Encoder_GetByteType(encoder, text, index);
}

/** Calls libHaru's HPDF_Encoder_GetUnicode with C89 facade types. */
CPKT_PDF_UNICODE cpkt_pdf_encoder_get_unicode(CPKT_PDF_Encoder encoder,
                                              CPKT_PDF_UINT16 code) {
  return HPDF_Encoder_GetUnicode(encoder, code);
}

/** Calls libHaru's HPDF_Encoder_GetWritingMode with C89 facade types. */
CPKT_PDF_WritingMode
cpkt_pdf_encoder_get_writing_mode(CPKT_PDF_Encoder encoder) {
  return (CPKT_PDF_WritingMode)HPDF_Encoder_GetWritingMode(encoder);
}

/** Calls libHaru's HPDF_UseJPEncodings with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_jp_encodings(CPKT_PDF_Doc pdf) {
  return HPDF_UseJPEncodings(pdf);
}

/** Calls libHaru's HPDF_UseKREncodings with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_kr_encodings(CPKT_PDF_Doc pdf) {
  return HPDF_UseKREncodings(pdf);
}

/** Calls libHaru's HPDF_UseCNSEncodings with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_cns_encodings(CPKT_PDF_Doc pdf) {
  return HPDF_UseCNSEncodings(pdf);
}

/** Calls libHaru's HPDF_UseCNTEncodings with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_cnt_encodings(CPKT_PDF_Doc pdf) {
  return HPDF_UseCNTEncodings(pdf);
}

/** Calls libHaru's HPDF_UseUTFEncodings with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_use_utf_encodings(CPKT_PDF_Doc pdf) {
  return HPDF_UseUTFEncodings(pdf);
}

/** Calls libHaru's HPDF_Page_CreateXObjectFromImage with C89 facade types. */
CPKT_PDF_XObject cpkt_pdf_page_create_x_object_from_image(CPKT_PDF_Doc pdf,
                                                          CPKT_PDF_Page page,
                                                          CPKT_PDF_Rect rect,
                                                          CPKT_PDF_Image image,
                                                          CPKT_PDF_BOOL zoom) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateXObjectFromImage(pdf, page, native_rect, image, zoom);
}

/** Calls libHaru's HPDF_Page_CreateXObjectAsWhiteRect with C89 facade types. */
CPKT_PDF_XObject cpkt_pdf_page_create_x_object_as_white_rect(
    CPKT_PDF_Doc pdf, CPKT_PDF_Page page, CPKT_PDF_Rect rect) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateXObjectAsWhiteRect(pdf, page, native_rect);
}

/** Calls libHaru's HPDF_Page_Create3DAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create3_d_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                              CPKT_PDF_BOOL tb, CPKT_PDF_BOOL np,
                              CPKT_PDF_U3D u3d, CPKT_PDF_Image ap) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_Create3DAnnot(page, native_rect, tb, np, u3d, ap);
}

/** Calls libHaru's HPDF_Page_CreateTextAnnot with C89 facade types. */
CPKT_PDF_Annotation cpkt_pdf_page_create_text_annot(CPKT_PDF_Page page,
                                                    CPKT_PDF_Rect rect,
                                                    const char *text,
                                                    CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateTextAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateFreeTextAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_free_text_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                     const char *text,
                                     CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateFreeTextAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateLineAnnot with C89 facade types. */
CPKT_PDF_Annotation cpkt_pdf_page_create_line_annot(CPKT_PDF_Page page,
                                                    const char *text,
                                                    CPKT_PDF_Encoder encoder) {
  return HPDF_Page_CreateLineAnnot(page, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateWidgetAnnot_WhiteOnlyWhilePrint with C89
 * facade types. */
CPKT_PDF_Annotation cpkt_pdf_page_create_widget_annot_white_only_while_print(
    CPKT_PDF_Doc pdf, CPKT_PDF_Page page, CPKT_PDF_Rect rect) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateWidgetAnnot_WhiteOnlyWhilePrint(pdf, page,
                                                         native_rect);
}

/** Calls libHaru's HPDF_Page_CreateWidgetAnnot with C89 facade types. */
CPKT_PDF_Annotation cpkt_pdf_page_create_widget_annot(CPKT_PDF_Page page,
                                                      CPKT_PDF_Rect rect) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateWidgetAnnot(page, native_rect);
}

/** Calls libHaru's HPDF_Page_CreateLinkAnnot with C89 facade types. */
CPKT_PDF_Annotation cpkt_pdf_page_create_link_annot(CPKT_PDF_Page page,
                                                    CPKT_PDF_Rect rect,
                                                    CPKT_PDF_Destination dst) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateLinkAnnot(page, native_rect, dst);
}

/** Calls libHaru's HPDF_Page_CreateURILinkAnnot with C89 facade types. */
CPKT_PDF_Annotation cpkt_pdf_page_create_uri_link_annot(CPKT_PDF_Page page,
                                                        CPKT_PDF_Rect rect,
                                                        const char *uri) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateURILinkAnnot(page, native_rect, uri);
}

/** Calls libHaru's HPDF_Page_CreateHighlightAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_highlight_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                     const char *text,
                                     CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateHighlightAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateUnderlineAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_underline_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                     const char *text,
                                     CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateUnderlineAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateSquigglyAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_squiggly_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                    const char *text,
                                    CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateSquigglyAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateStrikeOutAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_strike_out_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                      const char *text,
                                      CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateStrikeOutAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreatePopupAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_popup_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                 CPKT_PDF_Annotation parent) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreatePopupAnnot(page, native_rect, parent);
}

/** Calls libHaru's HPDF_Page_CreateStampAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_stamp_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                 CPKT_PDF_StampAnnotName name, const char *text,
                                 CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateStampAnnot(page, native_rect,
                                    (HPDF_StampAnnotName)name, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateProjectionAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_projection_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                      const char *text,
                                      CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateProjectionAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateSquareAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_square_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                  const char *text, CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateSquareAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_Page_CreateCircleAnnot with C89 facade types. */
CPKT_PDF_Annotation
cpkt_pdf_page_create_circle_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                  const char *text, CPKT_PDF_Encoder encoder) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_Page_CreateCircleAnnot(page, native_rect, text, encoder);
}

/** Calls libHaru's HPDF_LinkAnnot_SetHighlightMode with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_link_annot_set_highlight_mode(CPKT_PDF_Annotation annot,
                                       CPKT_PDF_AnnotHighlightMode mode) {
  return HPDF_LinkAnnot_SetHighlightMode(annot, (HPDF_AnnotHighlightMode)mode);
}

/** Calls libHaru's HPDF_LinkAnnot_SetJavaScript with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_link_annot_set_java_script(CPKT_PDF_Annotation annot,
                                    CPKT_PDF_JavaScript javascript) {
  return HPDF_LinkAnnot_SetJavaScript(annot, javascript);
}

/** Calls libHaru's HPDF_LinkAnnot_SetBorderStyle with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_link_annot_set_border_style(CPKT_PDF_Annotation annot,
                                                     CPKT_PDF_REAL width,
                                                     CPKT_PDF_UINT16 dash_on,
                                                     CPKT_PDF_UINT16 dash_off) {
  return HPDF_LinkAnnot_SetBorderStyle(annot, width, dash_on, dash_off);
}

/** Calls libHaru's HPDF_TextAnnot_SetIcon with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_text_annot_set_icon(CPKT_PDF_Annotation annot,
                                             CPKT_PDF_AnnotIcon icon) {
  return HPDF_TextAnnot_SetIcon(annot, (HPDF_AnnotIcon)icon);
}

/** Calls libHaru's HPDF_TextAnnot_SetOpened with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_text_annot_set_opened(CPKT_PDF_Annotation annot,
                                               CPKT_PDF_BOOL opened) {
  return HPDF_TextAnnot_SetOpened(annot, opened);
}

/** Calls libHaru's HPDF_Annot_SetRGBColor with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_annot_set_rgb_color(CPKT_PDF_Annotation annot,
                                             CPKT_PDF_RGBColor color) {
  HPDF_RGBColor native_color;
  memcpy(&native_color, &color, sizeof(native_color));
  return HPDF_Annot_SetRGBColor(annot, native_color);
}

/** Calls libHaru's HPDF_Annot_SetCMYKColor with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_annot_set_cmyk_color(CPKT_PDF_Annotation annot,
                                              CPKT_PDF_CMYKColor color) {
  HPDF_CMYKColor native_color;
  memcpy(&native_color, &color, sizeof(native_color));
  return HPDF_Annot_SetCMYKColor(annot, native_color);
}

/** Calls libHaru's HPDF_Annot_SetGrayColor with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_annot_set_gray_color(CPKT_PDF_Annotation annot,
                                              CPKT_PDF_REAL color) {
  return HPDF_Annot_SetGrayColor(annot, color);
}

/** Calls libHaru's HPDF_Annot_SetNoColor with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_annot_set_no_color(CPKT_PDF_Annotation annot) {
  return HPDF_Annot_SetNoColor(annot);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetTitle with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_title(CPKT_PDF_Annotation annot,
                                                const char *name) {
  return HPDF_MarkupAnnot_SetTitle(annot, name);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetSubject with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_subject(CPKT_PDF_Annotation annot,
                                                  const char *name) {
  return HPDF_MarkupAnnot_SetSubject(annot, name);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetCreationDate with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_creation_date(CPKT_PDF_Annotation annot,
                                        CPKT_PDF_Date value) {
  HPDF_Date native_value;
  memcpy(&native_value, &value, sizeof(native_value));
  return HPDF_MarkupAnnot_SetCreationDate(annot, native_value);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetTransparency with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_transparency(CPKT_PDF_Annotation annot,
                                       CPKT_PDF_REAL value) {
  return HPDF_MarkupAnnot_SetTransparency(annot, value);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetIntent with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_intent(CPKT_PDF_Annotation annot,
                                                 CPKT_PDF_AnnotIntent intent) {
  return HPDF_MarkupAnnot_SetIntent(annot, (HPDF_AnnotIntent)intent);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetPopup with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_popup(CPKT_PDF_Annotation annot,
                                                CPKT_PDF_Annotation popup) {
  return HPDF_MarkupAnnot_SetPopup(annot, popup);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetRectDiff with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_rect_diff(CPKT_PDF_Annotation annot,
                                                    CPKT_PDF_Rect rect) {
  HPDF_Rect native_rect;
  memcpy(&native_rect, &rect, sizeof(native_rect));
  return HPDF_MarkupAnnot_SetRectDiff(annot, native_rect);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetCloudEffect with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_cloud_effect(CPKT_PDF_Annotation annot,
                                       CPKT_PDF_INT cloudIntensity) {
  return HPDF_MarkupAnnot_SetCloudEffect(annot, cloudIntensity);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorRGBColor with C89 facade types.
 */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_interior_rgb_color(CPKT_PDF_Annotation annot,
                                             CPKT_PDF_RGBColor color) {
  HPDF_RGBColor native_color;
  memcpy(&native_color, &color, sizeof(native_color));
  return HPDF_MarkupAnnot_SetInteriorRGBColor(annot, native_color);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorCMYKColor with C89 facade types.
 */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_interior_cmyk_color(CPKT_PDF_Annotation annot,
                                              CPKT_PDF_CMYKColor color) {
  HPDF_CMYKColor native_color;
  memcpy(&native_color, &color, sizeof(native_color));
  return HPDF_MarkupAnnot_SetInteriorCMYKColor(annot, native_color);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorGrayColor with C89 facade types.
 */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_interior_gray_color(CPKT_PDF_Annotation annot,
                                              CPKT_PDF_REAL color) {
  return HPDF_MarkupAnnot_SetInteriorGrayColor(annot, color);
}

/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorTransparent with C89 facade
 * types. */
CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_interior_transparent(CPKT_PDF_Annotation annot) {
  return HPDF_MarkupAnnot_SetInteriorTransparent(annot);
}

/** Calls libHaru's HPDF_TextMarkupAnnot_SetQuadPoints with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_text_markup_annot_set_quad_points(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point lb, CPKT_PDF_Point rb,
    CPKT_PDF_Point rt, CPKT_PDF_Point lt) {
  HPDF_Point native_lb;
  HPDF_Point native_rb;
  HPDF_Point native_rt;
  HPDF_Point native_lt;
  memcpy(&native_lb, &lb, sizeof(native_lb));
  memcpy(&native_rb, &rb, sizeof(native_rb));
  memcpy(&native_rt, &rt, sizeof(native_rt));
  memcpy(&native_lt, &lt, sizeof(native_lt));
  return HPDF_TextMarkupAnnot_SetQuadPoints(annot, native_lb, native_rb,
                                            native_rt, native_lt);
}

/** Calls libHaru's HPDF_Annot_Set3DView with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_annot_set3_d_view(CPKT_PDF_MMgr mmgr,
                                           CPKT_PDF_Annotation annot,
                                           CPKT_PDF_Annotation annot3d,
                                           CPKT_PDF_Dict view) {
  return HPDF_Annot_Set3DView(mmgr, annot, annot3d, view);
}

/** Calls libHaru's HPDF_PopupAnnot_SetOpened with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_popup_annot_set_opened(CPKT_PDF_Annotation annot,
                                                CPKT_PDF_BOOL opened) {
  return HPDF_PopupAnnot_SetOpened(annot, opened);
}

/** Calls libHaru's HPDF_FreeTextAnnot_SetLineEndingStyle with C89 facade types.
 */
CPKT_PDF_STATUS cpkt_pdf_free_text_annot_set_line_ending_style(
    CPKT_PDF_Annotation annot, CPKT_PDF_LineAnnotEndingStyle startStyle,
    CPKT_PDF_LineAnnotEndingStyle endStyle) {
  return HPDF_FreeTextAnnot_SetLineEndingStyle(
      annot, (HPDF_LineAnnotEndingStyle)startStyle,
      (HPDF_LineAnnotEndingStyle)endStyle);
}

/** Calls libHaru's HPDF_FreeTextAnnot_Set3PointCalloutLine with C89 facade
 * types. */
CPKT_PDF_STATUS cpkt_pdf_free_text_annot_set3_point_callout_line(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point startPoint,
    CPKT_PDF_Point kneePoint, CPKT_PDF_Point endPoint) {
  HPDF_Point native_startPoint;
  HPDF_Point native_kneePoint;
  HPDF_Point native_endPoint;
  memcpy(&native_startPoint, &startPoint, sizeof(native_startPoint));
  memcpy(&native_kneePoint, &kneePoint, sizeof(native_kneePoint));
  memcpy(&native_endPoint, &endPoint, sizeof(native_endPoint));
  return HPDF_FreeTextAnnot_Set3PointCalloutLine(
      annot, native_startPoint, native_kneePoint, native_endPoint);
}

/** Calls libHaru's HPDF_FreeTextAnnot_Set2PointCalloutLine with C89 facade
 * types. */
CPKT_PDF_STATUS
cpkt_pdf_free_text_annot_set2_point_callout_line(CPKT_PDF_Annotation annot,
                                                 CPKT_PDF_Point startPoint,
                                                 CPKT_PDF_Point endPoint) {
  HPDF_Point native_startPoint;
  HPDF_Point native_endPoint;
  memcpy(&native_startPoint, &startPoint, sizeof(native_startPoint));
  memcpy(&native_endPoint, &endPoint, sizeof(native_endPoint));
  return HPDF_FreeTextAnnot_Set2PointCalloutLine(annot, native_startPoint,
                                                 native_endPoint);
}

/** Calls libHaru's HPDF_FreeTextAnnot_SetDefaultStyle with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_free_text_annot_set_default_style(CPKT_PDF_Annotation annot,
                                           const char *style) {
  return HPDF_FreeTextAnnot_SetDefaultStyle(annot, style);
}

/** Calls libHaru's HPDF_LineAnnot_SetPosition with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_line_annot_set_position(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point startPoint,
    CPKT_PDF_LineAnnotEndingStyle startStyle, CPKT_PDF_Point endPoint,
    CPKT_PDF_LineAnnotEndingStyle endStyle) {
  HPDF_Point native_startPoint;
  HPDF_Point native_endPoint;
  memcpy(&native_startPoint, &startPoint, sizeof(native_startPoint));
  memcpy(&native_endPoint, &endPoint, sizeof(native_endPoint));
  return HPDF_LineAnnot_SetPosition(
      annot, native_startPoint, (HPDF_LineAnnotEndingStyle)startStyle,
      native_endPoint, (HPDF_LineAnnotEndingStyle)endStyle);
}

/** Calls libHaru's HPDF_LineAnnot_SetLeader with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_line_annot_set_leader(CPKT_PDF_Annotation annot,
                                               CPKT_PDF_INT leaderLen,
                                               CPKT_PDF_INT leaderExtLen,
                                               CPKT_PDF_INT leaderOffsetLen) {
  return HPDF_LineAnnot_SetLeader(annot, leaderLen, leaderExtLen,
                                  leaderOffsetLen);
}

/** Calls libHaru's HPDF_LineAnnot_SetCaption with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_line_annot_set_caption(
    CPKT_PDF_Annotation annot, CPKT_PDF_BOOL showCaption,
    CPKT_PDF_LineAnnotCapPosition position, CPKT_PDF_INT horzOffset,
    CPKT_PDF_INT vertOffset) {
  return HPDF_LineAnnot_SetCaption(annot, showCaption,
                                   (HPDF_LineAnnotCapPosition)position,
                                   horzOffset, vertOffset);
}

/** Calls libHaru's HPDF_Annotation_SetBorderStyle with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_annotation_set_border_style(
    CPKT_PDF_Annotation annot, CPKT_PDF_BSSubtype subtype, CPKT_PDF_REAL width,
    CPKT_PDF_UINT16 dash_on, CPKT_PDF_UINT16 dash_off,
    CPKT_PDF_UINT16 dash_phase) {
  return HPDF_Annotation_SetBorderStyle(annot, (HPDF_BSSubtype)subtype, width,
                                        dash_on, dash_off, dash_phase);
}

/** Calls libHaru's HPDF_ProjectionAnnot_SetExData with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_projection_annot_set_ex_data(CPKT_PDF_Annotation annot,
                                                      CPKT_PDF_ExData exdata) {
  return HPDF_ProjectionAnnot_SetExData(annot, exdata);
}

/** Calls libHaru's HPDF_Page_Create3DC3DMeasure with C89 facade types. */
CPKT_PDF_3DMeasure
cpkt_pdf_page_create3_dc3_d_measure(CPKT_PDF_Page page,
                                    CPKT_PDF_Point3D firstanchorpoint,
                                    CPKT_PDF_Point3D textanchorpoint) {
  HPDF_Point3D native_firstanchorpoint;
  HPDF_Point3D native_textanchorpoint;
  memcpy(&native_firstanchorpoint, &firstanchorpoint,
         sizeof(native_firstanchorpoint));
  memcpy(&native_textanchorpoint, &textanchorpoint,
         sizeof(native_textanchorpoint));
  return HPDF_Page_Create3DC3DMeasure(page, native_firstanchorpoint,
                                      native_textanchorpoint);
}

/** Calls libHaru's HPDF_Page_CreatePD33DMeasure with C89 facade types. */
CPKT_PDF_3DMeasure cpkt_pdf_page_create_pd33_d_measure(
    CPKT_PDF_Page page, CPKT_PDF_Point3D annotationPlaneNormal,
    CPKT_PDF_Point3D firstAnchorPoint, CPKT_PDF_Point3D secondAnchorPoint,
    CPKT_PDF_Point3D leaderLinesDirection,
    CPKT_PDF_Point3D measurementValuePoint, CPKT_PDF_Point3D textYDirection,
    CPKT_PDF_REAL value, const char *unitsString) {
  HPDF_Point3D native_annotationPlaneNormal;
  HPDF_Point3D native_firstAnchorPoint;
  HPDF_Point3D native_secondAnchorPoint;
  HPDF_Point3D native_leaderLinesDirection;
  HPDF_Point3D native_measurementValuePoint;
  HPDF_Point3D native_textYDirection;
  memcpy(&native_annotationPlaneNormal, &annotationPlaneNormal,
         sizeof(native_annotationPlaneNormal));
  memcpy(&native_firstAnchorPoint, &firstAnchorPoint,
         sizeof(native_firstAnchorPoint));
  memcpy(&native_secondAnchorPoint, &secondAnchorPoint,
         sizeof(native_secondAnchorPoint));
  memcpy(&native_leaderLinesDirection, &leaderLinesDirection,
         sizeof(native_leaderLinesDirection));
  memcpy(&native_measurementValuePoint, &measurementValuePoint,
         sizeof(native_measurementValuePoint));
  memcpy(&native_textYDirection, &textYDirection,
         sizeof(native_textYDirection));
  return HPDF_Page_CreatePD33DMeasure(
      page, native_annotationPlaneNormal, native_firstAnchorPoint,
      native_secondAnchorPoint, native_leaderLinesDirection,
      native_measurementValuePoint, native_textYDirection, value, unitsString);
}

/** Calls libHaru's HPDF_3DMeasure_SetName with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_3_d_measure_set_name(CPKT_PDF_3DMeasure measure,
                                              const char *name) {
  return HPDF_3DMeasure_SetName(measure, name);
}

/** Calls libHaru's HPDF_3DMeasure_SetColor with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_3_d_measure_set_color(CPKT_PDF_3DMeasure measure,
                                               CPKT_PDF_RGBColor color) {
  HPDF_RGBColor native_color;
  memcpy(&native_color, &color, sizeof(native_color));
  return HPDF_3DMeasure_SetColor(measure, native_color);
}

/** Calls libHaru's HPDF_3DMeasure_SetTextSize with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_3_d_measure_set_text_size(CPKT_PDF_3DMeasure measure,
                                                   CPKT_PDF_REAL textsize) {
  return HPDF_3DMeasure_SetTextSize(measure, textsize);
}

/** Calls libHaru's HPDF_3DC3DMeasure_SetTextBoxSize with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_3_dc3_d_measure_set_text_box_size(CPKT_PDF_3DMeasure measure,
                                           CPKT_PDF_INT32 x, CPKT_PDF_INT32 y) {
  return HPDF_3DC3DMeasure_SetTextBoxSize(measure, x, y);
}

/** Calls libHaru's HPDF_3DC3DMeasure_SetText with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_3_dc3_d_measure_set_text(CPKT_PDF_3DMeasure measure,
                                                  const char *text,
                                                  CPKT_PDF_Encoder encoder) {
  return HPDF_3DC3DMeasure_SetText(measure, text, encoder);
}

/** Calls libHaru's HPDF_3DC3DMeasure_SetProjectionAnotation with C89 facade
 * types. */
CPKT_PDF_STATUS cpkt_pdf_3_dc3_d_measure_set_projection_anotation(
    CPKT_PDF_3DMeasure measure, CPKT_PDF_Annotation projectionanotation) {
  return HPDF_3DC3DMeasure_SetProjectionAnotation(measure, projectionanotation);
}

/** Calls libHaru's HPDF_Page_Create3DAnnotExData with C89 facade types. */
CPKT_PDF_ExData cpkt_pdf_page_create3_d_annot_ex_data(CPKT_PDF_Page page) {
  return HPDF_Page_Create3DAnnotExData(page);
}

/** Calls libHaru's HPDF_3DAnnotExData_Set3DMeasurement with C89 facade types.
 */
CPKT_PDF_STATUS
cpkt_pdf_3_d_annot_ex_data_set3_d_measurement(CPKT_PDF_ExData exdata,
                                              CPKT_PDF_3DMeasure measure) {
  return HPDF_3DAnnotExData_Set3DMeasurement(exdata, measure);
}

/** Calls libHaru's HPDF_Page_Create3DView with C89 facade types. */
CPKT_PDF_Dict cpkt_pdf_page_create3_d_view(CPKT_PDF_Page page, CPKT_PDF_U3D u3d,
                                           CPKT_PDF_Annotation annot3d,
                                           const char *name) {
  return HPDF_Page_Create3DView(page, u3d, annot3d, name);
}

/** Calls libHaru's HPDF_3DView_Add3DC3DMeasure with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_3_d_view_add3_dc3_d_measure(CPKT_PDF_Dict view,
                                     CPKT_PDF_3DMeasure measure) {
  return HPDF_3DView_Add3DC3DMeasure(view, measure);
}

/** Loads a PNG byte buffer as a document-owned image. */
CPKT_PDF_Image cpkt_pdf_load_png_image_from_mem(CPKT_PDF_Doc pdf,
                                                const CPKT_PDF_BYTE *buffer,
                                                CPKT_PDF_UINT size) {
  return HPDF_LoadPngImageFromMem(pdf, buffer, size);
}

/** Loads a PNG file as a document-owned image. */
CPKT_PDF_Image cpkt_pdf_load_png_image_from_file(CPKT_PDF_Doc pdf,
                                                 const char *filename) {
  return HPDF_LoadPngImageFromFile(pdf, filename);
}

/** Loads a PNG file as a document-owned image with delayed image-data loading.
 */
CPKT_PDF_Image cpkt_pdf_load_png_image_from_file2(CPKT_PDF_Doc pdf,
                                                  const char *filename) {
  return HPDF_LoadPngImageFromFile2(pdf, filename);
}

/** Calls libHaru's HPDF_LoadJpegImageFromFile with C89 facade types. */
CPKT_PDF_Image cpkt_pdf_load_jpeg_image_from_file(CPKT_PDF_Doc pdf,
                                                  const char *filename) {
  return HPDF_LoadJpegImageFromFile(pdf, filename);
}

/** Calls libHaru's HPDF_LoadJpegImageFromMem with C89 facade types. */
CPKT_PDF_Image cpkt_pdf_load_jpeg_image_from_mem(CPKT_PDF_Doc pdf,
                                                 const CPKT_PDF_BYTE *buffer,
                                                 CPKT_PDF_UINT size) {
  return HPDF_LoadJpegImageFromMem(pdf, buffer, size);
}

/** Calls libHaru's HPDF_LoadU3DFromFile with C89 facade types. */
CPKT_PDF_Image cpkt_pdf_load_u3_d_from_file(CPKT_PDF_Doc pdf,
                                            const char *filename) {
  return HPDF_LoadU3DFromFile(pdf, filename);
}

/** Calls libHaru's HPDF_LoadU3DFromMem with C89 facade types. */
CPKT_PDF_Image cpkt_pdf_load_u3_d_from_mem(CPKT_PDF_Doc pdf,
                                           const CPKT_PDF_BYTE *buffer,
                                           CPKT_PDF_UINT size) {
  return HPDF_LoadU3DFromMem(pdf, buffer, size);
}

/** Calls libHaru's HPDF_Image_LoadRaw1BitImageFromMem with C89 facade types. */
CPKT_PDF_Image cpkt_pdf_image_load_raw1_bit_image_from_mem(
    CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buf, CPKT_PDF_UINT width,
    CPKT_PDF_UINT height, CPKT_PDF_UINT line_width, CPKT_PDF_BOOL black_is1,
    CPKT_PDF_BOOL top_is_first) {
  return HPDF_Image_LoadRaw1BitImageFromMem(pdf, buf, width, height, line_width,
                                            black_is1, top_is_first);
}

/** Calls libHaru's HPDF_LoadRawImageFromFile with C89 facade types. */
CPKT_PDF_Image
cpkt_pdf_load_raw_image_from_file(CPKT_PDF_Doc pdf, const char *filename,
                                  CPKT_PDF_UINT width, CPKT_PDF_UINT height,
                                  CPKT_PDF_ColorSpace color_space) {
  return HPDF_LoadRawImageFromFile(pdf, filename, width, height,
                                   (HPDF_ColorSpace)color_space);
}

/** Calls libHaru's HPDF_LoadRawImageFromMem with C89 facade types. */
CPKT_PDF_Image
cpkt_pdf_load_raw_image_from_mem(CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buf,
                                 CPKT_PDF_UINT width, CPKT_PDF_UINT height,
                                 CPKT_PDF_ColorSpace color_space,
                                 CPKT_PDF_UINT bits_per_component) {
  return HPDF_LoadRawImageFromMem(pdf, buf, width, height,
                                  (HPDF_ColorSpace)color_space,
                                  bits_per_component);
}

/** Calls libHaru's HPDF_Image_AddSMask with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_image_add_s_mask(CPKT_PDF_Image image,
                                          CPKT_PDF_Image smask) {
  return HPDF_Image_AddSMask(image, smask);
}

/** Calls libHaru's HPDF_Image_GetSize with C89 facade types. */
CPKT_PDF_Point cpkt_pdf_image_get_size(CPKT_PDF_Image image) {
  HPDF_Point native_result;
  CPKT_PDF_Point result;
  native_result = HPDF_Image_GetSize(image);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Image_GetSize2 with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_image_get_size2(CPKT_PDF_Image image,
                                         CPKT_PDF_Point *size) {
  HPDF_Point native_size;
  HPDF_STATUS result;
  result = HPDF_Image_GetSize2(image, size ? &native_size : NULL);
  if (size)
    memcpy(size, &native_size, sizeof(*size));
  return result;
}

/** Calls libHaru's HPDF_Image_GetWidth with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_image_get_width(CPKT_PDF_Image image) {
  return HPDF_Image_GetWidth(image);
}

/** Calls libHaru's HPDF_Image_GetHeight with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_image_get_height(CPKT_PDF_Image image) {
  return HPDF_Image_GetHeight(image);
}

/** Calls libHaru's HPDF_Image_GetBitsPerComponent with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_image_get_bits_per_component(CPKT_PDF_Image image) {
  return HPDF_Image_GetBitsPerComponent(image);
}

/** Calls libHaru's HPDF_Image_GetColorSpace with C89 facade types. */
const char *cpkt_pdf_image_get_color_space(CPKT_PDF_Image image) {
  return HPDF_Image_GetColorSpace(image);
}

/** Calls libHaru's HPDF_Image_SetColorMask with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_image_set_color_mask(CPKT_PDF_Image image, CPKT_PDF_UINT rmin,
                              CPKT_PDF_UINT rmax, CPKT_PDF_UINT gmin,
                              CPKT_PDF_UINT gmax, CPKT_PDF_UINT bmin,
                              CPKT_PDF_UINT bmax) {
  return HPDF_Image_SetColorMask(image, rmin, rmax, gmin, gmax, bmin, bmax);
}

/** Calls libHaru's HPDF_Image_SetMaskImage with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_image_set_mask_image(CPKT_PDF_Image image,
                                              CPKT_PDF_Image mask_image) {
  return HPDF_Image_SetMaskImage(image, mask_image);
}

/** Calls libHaru's HPDF_SetInfoAttr with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_info_attr(CPKT_PDF_Doc pdf, CPKT_PDF_InfoType type,
                                       const char *value) {
  return HPDF_SetInfoAttr(pdf, (HPDF_InfoType)type, value);
}

/** Calls libHaru's HPDF_GetInfoAttr with C89 facade types. */
const char *cpkt_pdf_get_info_attr(CPKT_PDF_Doc pdf, CPKT_PDF_InfoType type) {
  return HPDF_GetInfoAttr(pdf, (HPDF_InfoType)type);
}

/** Calls libHaru's HPDF_SetInfoDateAttr with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_info_date_attr(CPKT_PDF_Doc pdf,
                                            CPKT_PDF_InfoType type,
                                            CPKT_PDF_Date value) {
  HPDF_Date native_value;
  memcpy(&native_value, &value, sizeof(native_value));
  return HPDF_SetInfoDateAttr(pdf, (HPDF_InfoType)type, native_value);
}

/** Calls libHaru's HPDF_SetPassword with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_password(CPKT_PDF_Doc pdf,
                                      const char *owner_passwd,
                                      const char *user_passwd) {
  return HPDF_SetPassword(pdf, owner_passwd, user_passwd);
}

/** Calls libHaru's HPDF_SetPermission with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_permission(CPKT_PDF_Doc pdf,
                                        CPKT_PDF_UINT permission) {
  return HPDF_SetPermission(pdf, permission);
}

/** Calls libHaru's HPDF_SetEncryptionMode with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_encryption_mode(CPKT_PDF_Doc pdf,
                                             CPKT_PDF_EncryptMode mode,
                                             CPKT_PDF_UINT key_len) {
  return HPDF_SetEncryptionMode(pdf, (HPDF_EncryptMode)mode, key_len);
}

/** Calls libHaru's HPDF_SetCompressionMode with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_set_compression_mode(CPKT_PDF_Doc pdf,
                                              CPKT_PDF_UINT mode) {
  return HPDF_SetCompressionMode(pdf, mode);
}

/** Calls libHaru's HPDF_Font_GetFontName with C89 facade types. */
const char *cpkt_pdf_font_get_font_name(CPKT_PDF_Font font) {
  return HPDF_Font_GetFontName(font);
}

/** Calls libHaru's HPDF_Font_GetEncodingName with C89 facade types. */
const char *cpkt_pdf_font_get_encoding_name(CPKT_PDF_Font font) {
  return HPDF_Font_GetEncodingName(font);
}

/** Calls libHaru's HPDF_Font_GetUnicodeWidth with C89 facade types. */
CPKT_PDF_INT cpkt_pdf_font_get_unicode_width(CPKT_PDF_Font font,
                                             CPKT_PDF_UNICODE code) {
  return HPDF_Font_GetUnicodeWidth(font, code);
}

/** Calls libHaru's HPDF_Font_GetBBox with C89 facade types. */
CPKT_PDF_Box cpkt_pdf_font_get_b_box(CPKT_PDF_Font font) {
  HPDF_Box native_result;
  CPKT_PDF_Box result;
  native_result = HPDF_Font_GetBBox(font);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Font_GetAscent with C89 facade types. */
CPKT_PDF_INT cpkt_pdf_font_get_ascent(CPKT_PDF_Font font) {
  return HPDF_Font_GetAscent(font);
}

/** Calls libHaru's HPDF_Font_GetDescent with C89 facade types. */
CPKT_PDF_INT cpkt_pdf_font_get_descent(CPKT_PDF_Font font) {
  return HPDF_Font_GetDescent(font);
}

/** Calls libHaru's HPDF_Font_GetXHeight with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_font_get_x_height(CPKT_PDF_Font font) {
  return HPDF_Font_GetXHeight(font);
}

/** Calls libHaru's HPDF_Font_GetCapHeight with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_font_get_cap_height(CPKT_PDF_Font font) {
  return HPDF_Font_GetCapHeight(font);
}

/** Calls libHaru's HPDF_Font_TextWidth with C89 facade types. */
CPKT_PDF_TextWidth cpkt_pdf_font_text_width(CPKT_PDF_Font font,
                                            const CPKT_PDF_BYTE *text,
                                            CPKT_PDF_UINT len) {
  HPDF_TextWidth native_result;
  CPKT_PDF_TextWidth result;
  native_result = HPDF_Font_TextWidth(font, text, len);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Font_MeasureText with C89 facade types. */
CPKT_PDF_UINT
cpkt_pdf_font_measure_text(CPKT_PDF_Font font, const CPKT_PDF_BYTE *text,
                           CPKT_PDF_UINT len, CPKT_PDF_REAL width,
                           CPKT_PDF_REAL font_size, CPKT_PDF_REAL char_space,
                           CPKT_PDF_REAL word_space, CPKT_PDF_BOOL wordwrap,
                           CPKT_PDF_REAL *real_width) {
  return HPDF_Font_MeasureText(font, text, len, width, font_size, char_space,
                               word_space, wordwrap, real_width);
}

/** Calls libHaru's HPDF_AttachFile with C89 facade types. */
CPKT_PDF_EmbeddedFile cpkt_pdf_attach_file(CPKT_PDF_Doc pdf, const char *file) {
  return HPDF_AttachFile(pdf, file);
}

/** Calls libHaru's HPDF_EmbeddedFile_SetName with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_name(CPKT_PDF_EmbeddedFile emfile,
                                                const char *name) {
  return HPDF_EmbeddedFile_SetName(emfile, name);
}

/** Calls libHaru's HPDF_EmbeddedFile_SetDescription with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_embedded_file_set_description(CPKT_PDF_EmbeddedFile emfile,
                                       const char *new_description) {
  return HPDF_EmbeddedFile_SetDescription(emfile, new_description);
}

/** Calls libHaru's HPDF_EmbeddedFile_SetSubtype with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_subtype(CPKT_PDF_EmbeddedFile emfile,
                                                   const char *subtype) {
  return HPDF_EmbeddedFile_SetSubtype(emfile, subtype);
}

/** Calls libHaru's HPDF_EmbeddedFile_SetAFRelationship with C89 facade types.
 */
CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_af_relationship(
    CPKT_PDF_EmbeddedFile emfile, CPKT_PDF_AFRelationship relationship) {
  return HPDF_EmbeddedFile_SetAFRelationship(emfile,
                                             (HPDF_AFRelationship)relationship);
}

/** Calls libHaru's HPDF_EmbeddedFile_SetSize with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_size(CPKT_PDF_EmbeddedFile emfile,
                                                CPKT_PDF_UINT64 size) {
  return HPDF_EmbeddedFile_SetSize(emfile,
                                   (((HPDF_UINT64)size.hi << 32) | size.lo));
}

/** Calls libHaru's HPDF_EmbeddedFile_SetCreationDate with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_embedded_file_set_creation_date(CPKT_PDF_EmbeddedFile emfile,
                                         CPKT_PDF_Date creationDate) {
  HPDF_Date native_creationDate;
  memcpy(&native_creationDate, &creationDate, sizeof(native_creationDate));
  return HPDF_EmbeddedFile_SetCreationDate(emfile, native_creationDate);
}

/** Calls libHaru's HPDF_EmbeddedFile_SetLastModificationDate with C89 facade
 * types. */
CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_last_modification_date(
    CPKT_PDF_EmbeddedFile emfile, CPKT_PDF_Date lastModificationDate) {
  HPDF_Date native_lastModificationDate;
  memcpy(&native_lastModificationDate, &lastModificationDate,
         sizeof(native_lastModificationDate));
  return HPDF_EmbeddedFile_SetLastModificationDate(emfile,
                                                   native_lastModificationDate);
}

/** Calls libHaru's HPDF_CreateExtGState with C89 facade types. */
CPKT_PDF_ExtGState cpkt_pdf_create_ext_g_state(CPKT_PDF_Doc pdf) {
  return HPDF_CreateExtGState(pdf);
}

/** Calls libHaru's HPDF_ExtGState_SetAlphaStroke with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_ext_g_state_set_alpha_stroke(CPKT_PDF_ExtGState ext_gstate,
                                      CPKT_PDF_REAL value) {
  return HPDF_ExtGState_SetAlphaStroke(ext_gstate, value);
}

/** Calls libHaru's HPDF_ExtGState_SetAlphaFill with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_ext_g_state_set_alpha_fill(CPKT_PDF_ExtGState ext_gstate,
                                    CPKT_PDF_REAL value) {
  return HPDF_ExtGState_SetAlphaFill(ext_gstate, value);
}

/** Calls libHaru's HPDF_ExtGState_SetBlendMode with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_ext_g_state_set_blend_mode(CPKT_PDF_ExtGState ext_gstate,
                                    CPKT_PDF_BlendMode mode) {
  return HPDF_ExtGState_SetBlendMode(ext_gstate, (HPDF_BlendMode)mode);
}

/** Calls libHaru's HPDF_Page_TextWidth with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_text_width(CPKT_PDF_Page page, const char *text) {
  return HPDF_Page_TextWidth(page, text);
}

/** Calls libHaru's HPDF_Page_MeasureText with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_page_measure_text(CPKT_PDF_Page page, const char *text,
                                         CPKT_PDF_REAL width,
                                         CPKT_PDF_BOOL wordwrap,
                                         CPKT_PDF_REAL *real_width) {
  return HPDF_Page_MeasureText(page, text, width, wordwrap, real_width);
}

/** Calls libHaru's HPDF_Page_GetWidth with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_width(CPKT_PDF_Page page) {
  return HPDF_Page_GetWidth(page);
}

/** Calls libHaru's HPDF_Page_GetHeight with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_height(CPKT_PDF_Page page) {
  return HPDF_Page_GetHeight(page);
}

/** Calls libHaru's HPDF_Page_GetGMode with C89 facade types. */
CPKT_PDF_UINT16 cpkt_pdf_page_get_g_mode(CPKT_PDF_Page page) {
  return HPDF_Page_GetGMode(page);
}

/** Calls libHaru's HPDF_Page_GetCurrentPos with C89 facade types. */
CPKT_PDF_Point cpkt_pdf_page_get_current_pos(CPKT_PDF_Page page) {
  HPDF_Point native_result;
  CPKT_PDF_Point result;
  native_result = HPDF_Page_GetCurrentPos(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetCurrentPos2 with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_get_current_pos2(CPKT_PDF_Page page,
                                               CPKT_PDF_Point *pos) {
  HPDF_Point native_pos;
  HPDF_STATUS result;
  result = HPDF_Page_GetCurrentPos2(page, pos ? &native_pos : NULL);
  if (pos)
    memcpy(pos, &native_pos, sizeof(*pos));
  return result;
}

/** Calls libHaru's HPDF_Page_GetCurrentTextPos with C89 facade types. */
CPKT_PDF_Point cpkt_pdf_page_get_current_text_pos(CPKT_PDF_Page page) {
  HPDF_Point native_result;
  CPKT_PDF_Point result;
  native_result = HPDF_Page_GetCurrentTextPos(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetCurrentTextPos2 with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_get_current_text_pos2(CPKT_PDF_Page page,
                                                    CPKT_PDF_Point *pos) {
  HPDF_Point native_pos;
  HPDF_STATUS result;
  result = HPDF_Page_GetCurrentTextPos2(page, pos ? &native_pos : NULL);
  if (pos)
    memcpy(pos, &native_pos, sizeof(*pos));
  return result;
}

/** Calls libHaru's HPDF_Page_GetCurrentFont with C89 facade types. */
CPKT_PDF_Font cpkt_pdf_page_get_current_font(CPKT_PDF_Page page) {
  return HPDF_Page_GetCurrentFont(page);
}

/** Calls libHaru's HPDF_Page_GetCurrentFontSize with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_current_font_size(CPKT_PDF_Page page) {
  return HPDF_Page_GetCurrentFontSize(page);
}

/** Calls libHaru's HPDF_Page_GetTransMatrix with C89 facade types. */
CPKT_PDF_TransMatrix cpkt_pdf_page_get_trans_matrix(CPKT_PDF_Page page) {
  HPDF_TransMatrix native_result;
  CPKT_PDF_TransMatrix result;
  native_result = HPDF_Page_GetTransMatrix(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetLineWidth with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_line_width(CPKT_PDF_Page page) {
  return HPDF_Page_GetLineWidth(page);
}

/** Calls libHaru's HPDF_Page_GetLineCap with C89 facade types. */
CPKT_PDF_LineCap cpkt_pdf_page_get_line_cap(CPKT_PDF_Page page) {
  return (CPKT_PDF_LineCap)HPDF_Page_GetLineCap(page);
}

/** Calls libHaru's HPDF_Page_GetLineJoin with C89 facade types. */
CPKT_PDF_LineJoin cpkt_pdf_page_get_line_join(CPKT_PDF_Page page) {
  return (CPKT_PDF_LineJoin)HPDF_Page_GetLineJoin(page);
}

/** Calls libHaru's HPDF_Page_GetMiterLimit with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_miter_limit(CPKT_PDF_Page page) {
  return HPDF_Page_GetMiterLimit(page);
}

/** Calls libHaru's HPDF_Page_GetDash with C89 facade types. */
CPKT_PDF_DashMode cpkt_pdf_page_get_dash(CPKT_PDF_Page page) {
  HPDF_DashMode native_result;
  CPKT_PDF_DashMode result;
  native_result = HPDF_Page_GetDash(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetFlat with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_flat(CPKT_PDF_Page page) {
  return HPDF_Page_GetFlat(page);
}

/** Calls libHaru's HPDF_Page_GetCharSpace with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_char_space(CPKT_PDF_Page page) {
  return HPDF_Page_GetCharSpace(page);
}

/** Calls libHaru's HPDF_Page_GetWordSpace with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_word_space(CPKT_PDF_Page page) {
  return HPDF_Page_GetWordSpace(page);
}

/** Calls libHaru's HPDF_Page_GetHorizontalScalling with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_horizontal_scalling(CPKT_PDF_Page page) {
  return HPDF_Page_GetHorizontalScalling(page);
}

/** Calls libHaru's HPDF_Page_GetTextLeading with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_text_leading(CPKT_PDF_Page page) {
  return HPDF_Page_GetTextLeading(page);
}

/** Calls libHaru's HPDF_Page_GetTextRenderingMode with C89 facade types. */
CPKT_PDF_TextRenderingMode
cpkt_pdf_page_get_text_rendering_mode(CPKT_PDF_Page page) {
  return (CPKT_PDF_TextRenderingMode)HPDF_Page_GetTextRenderingMode(page);
}

/** Calls libHaru's HPDF_Page_GetTextRaise with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_text_raise(CPKT_PDF_Page page) {
  return HPDF_Page_GetTextRaise(page);
}

/** Calls libHaru's HPDF_Page_GetTextRise with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_text_rise(CPKT_PDF_Page page) {
  return HPDF_Page_GetTextRise(page);
}

/** Calls libHaru's HPDF_Page_GetRGBFill with C89 facade types. */
CPKT_PDF_RGBColor cpkt_pdf_page_get_rgb_fill(CPKT_PDF_Page page) {
  HPDF_RGBColor native_result;
  CPKT_PDF_RGBColor result;
  native_result = HPDF_Page_GetRGBFill(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetRGBStroke with C89 facade types. */
CPKT_PDF_RGBColor cpkt_pdf_page_get_rgb_stroke(CPKT_PDF_Page page) {
  HPDF_RGBColor native_result;
  CPKT_PDF_RGBColor result;
  native_result = HPDF_Page_GetRGBStroke(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetCMYKFill with C89 facade types. */
CPKT_PDF_CMYKColor cpkt_pdf_page_get_cmyk_fill(CPKT_PDF_Page page) {
  HPDF_CMYKColor native_result;
  CPKT_PDF_CMYKColor result;
  native_result = HPDF_Page_GetCMYKFill(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetCMYKStroke with C89 facade types. */
CPKT_PDF_CMYKColor cpkt_pdf_page_get_cmyk_stroke(CPKT_PDF_Page page) {
  HPDF_CMYKColor native_result;
  CPKT_PDF_CMYKColor result;
  native_result = HPDF_Page_GetCMYKStroke(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetGrayFill with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_gray_fill(CPKT_PDF_Page page) {
  return HPDF_Page_GetGrayFill(page);
}

/** Calls libHaru's HPDF_Page_GetGrayStroke with C89 facade types. */
CPKT_PDF_REAL cpkt_pdf_page_get_gray_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_GetGrayStroke(page);
}

/** Calls libHaru's HPDF_Page_GetStrokingColorSpace with C89 facade types. */
CPKT_PDF_ColorSpace cpkt_pdf_page_get_stroking_color_space(CPKT_PDF_Page page) {
  return (CPKT_PDF_ColorSpace)HPDF_Page_GetStrokingColorSpace(page);
}

/** Calls libHaru's HPDF_Page_GetFillingColorSpace with C89 facade types. */
CPKT_PDF_ColorSpace cpkt_pdf_page_get_filling_color_space(CPKT_PDF_Page page) {
  return (CPKT_PDF_ColorSpace)HPDF_Page_GetFillingColorSpace(page);
}

/** Calls libHaru's HPDF_Page_GetTextMatrix with C89 facade types. */
CPKT_PDF_TransMatrix cpkt_pdf_page_get_text_matrix(CPKT_PDF_Page page) {
  HPDF_TransMatrix native_result;
  CPKT_PDF_TransMatrix result;
  native_result = HPDF_Page_GetTextMatrix(page);
  memcpy(&result, &native_result, sizeof(result));
  return result;
}

/** Calls libHaru's HPDF_Page_GetGStateDepth with C89 facade types. */
CPKT_PDF_UINT cpkt_pdf_page_get_g_state_depth(CPKT_PDF_Page page) {
  return HPDF_Page_GetGStateDepth(page);
}

/** Calls libHaru's HPDF_Page_SetLineWidth with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_line_width(CPKT_PDF_Page page,
                                             CPKT_PDF_REAL line_width) {
  return HPDF_Page_SetLineWidth(page, line_width);
}

/** Calls libHaru's HPDF_Page_SetLineCap with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_line_cap(CPKT_PDF_Page page,
                                           CPKT_PDF_LineCap line_cap) {
  return HPDF_Page_SetLineCap(page, (HPDF_LineCap)line_cap);
}

/** Calls libHaru's HPDF_Page_SetLineJoin with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_line_join(CPKT_PDF_Page page,
                                            CPKT_PDF_LineJoin line_join) {
  return HPDF_Page_SetLineJoin(page, (HPDF_LineJoin)line_join);
}

/** Calls libHaru's HPDF_Page_SetMiterLimit with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_miter_limit(CPKT_PDF_Page page,
                                              CPKT_PDF_REAL miter_limit) {
  return HPDF_Page_SetMiterLimit(page, miter_limit);
}

/** Calls libHaru's HPDF_Page_SetDash with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_dash(CPKT_PDF_Page page,
                                       const CPKT_PDF_REAL *dash_ptn,
                                       CPKT_PDF_UINT num_param,
                                       CPKT_PDF_REAL phase) {
  return HPDF_Page_SetDash(page, dash_ptn, num_param, phase);
}

/** Calls libHaru's HPDF_Page_SetFlat with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_flat(CPKT_PDF_Page page,
                                       CPKT_PDF_REAL flatness) {
  return HPDF_Page_SetFlat(page, flatness);
}

/** Calls libHaru's HPDF_Page_SetExtGState with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_ext_g_state(CPKT_PDF_Page page,
                                              CPKT_PDF_ExtGState ext_gstate) {
  return HPDF_Page_SetExtGState(page, ext_gstate);
}

/** Calls libHaru's HPDF_Page_SetShading with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_shading(CPKT_PDF_Page page,
                                          CPKT_PDF_Shading shading) {
  return HPDF_Page_SetShading(page, shading);
}

/** Saves the page graphics state, including its transform and paint settings;
 * restore it with cpkt_pdf_page_g_restore(). */
CPKT_PDF_STATUS cpkt_pdf_page_g_save(CPKT_PDF_Page page) {
  return HPDF_Page_GSave(page);
}

/** Restores the most recently saved page graphics state. */
CPKT_PDF_STATUS cpkt_pdf_page_g_restore(CPKT_PDF_Page page) {
  return HPDF_Page_GRestore(page);
}

/** Concatenates an affine transform with the page's current transform; use
 * g_save/g_restore to limit its scope. */
CPKT_PDF_STATUS cpkt_pdf_page_concat(CPKT_PDF_Page page, CPKT_PDF_REAL a,
                                     CPKT_PDF_REAL b, CPKT_PDF_REAL c,
                                     CPKT_PDF_REAL d, CPKT_PDF_REAL x,
                                     CPKT_PDF_REAL y) {
  return HPDF_Page_Concat(page, a, b, c, d, x, y);
}

/** Starts a new path subpath at the given page coordinates. */
CPKT_PDF_STATUS cpkt_pdf_page_move_to(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                      CPKT_PDF_REAL y) {
  return HPDF_Page_MoveTo(page, x, y);
}

/** Adds a straight path segment from the current point. */
CPKT_PDF_STATUS cpkt_pdf_page_line_to(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                      CPKT_PDF_REAL y) {
  return HPDF_Page_LineTo(page, x, y);
}

/** Adds a cubic Bezier segment with two control points and an end point. */
CPKT_PDF_STATUS cpkt_pdf_page_curve_to(CPKT_PDF_Page page, CPKT_PDF_REAL x1,
                                       CPKT_PDF_REAL y1, CPKT_PDF_REAL x2,
                                       CPKT_PDF_REAL y2, CPKT_PDF_REAL x3,
                                       CPKT_PDF_REAL y3) {
  return HPDF_Page_CurveTo(page, x1, y1, x2, y2, x3, y3);
}

/** Adds a cubic Bezier segment whose first control point is the current point.
 */
CPKT_PDF_STATUS cpkt_pdf_page_curve_to2(CPKT_PDF_Page page, CPKT_PDF_REAL x2,
                                        CPKT_PDF_REAL y2, CPKT_PDF_REAL x3,
                                        CPKT_PDF_REAL y3) {
  return HPDF_Page_CurveTo2(page, x2, y2, x3, y3);
}

/** Adds a cubic Bezier segment whose second control point is the end point. */
CPKT_PDF_STATUS cpkt_pdf_page_curve_to3(CPKT_PDF_Page page, CPKT_PDF_REAL x1,
                                        CPKT_PDF_REAL y1, CPKT_PDF_REAL x3,
                                        CPKT_PDF_REAL y3) {
  return HPDF_Page_CurveTo3(page, x1, y1, x3, y3);
}

/** Closes the current path subpath. */
CPKT_PDF_STATUS cpkt_pdf_page_close_path(CPKT_PDF_Page page) {
  return HPDF_Page_ClosePath(page);
}

/** Adds a rectangle to the current path. */
CPKT_PDF_STATUS cpkt_pdf_page_rectangle(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                        CPKT_PDF_REAL y, CPKT_PDF_REAL width,
                                        CPKT_PDF_REAL height) {
  return HPDF_Page_Rectangle(page, x, y, width, height);
}

/** Strokes the current path with the page's stroke settings. */
CPKT_PDF_STATUS cpkt_pdf_page_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_Stroke(page);
}

/** Calls libHaru's HPDF_Page_ClosePathStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_close_path_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_ClosePathStroke(page);
}

/** Fills the current path with the nonzero winding rule. */
CPKT_PDF_STATUS cpkt_pdf_page_fill(CPKT_PDF_Page page) {
  return HPDF_Page_Fill(page);
}

/** Fills the current path with the even-odd rule. */
CPKT_PDF_STATUS cpkt_pdf_page_eofill(CPKT_PDF_Page page) {
  return HPDF_Page_Eofill(page);
}

/** Calls libHaru's HPDF_Page_FillStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_fill_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_FillStroke(page);
}

/** Calls libHaru's HPDF_Page_EofillStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_eofill_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_EofillStroke(page);
}

/** Calls libHaru's HPDF_Page_ClosePathFillStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_close_path_fill_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_ClosePathFillStroke(page);
}

/** Calls libHaru's HPDF_Page_ClosePathEofillStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_close_path_eofill_stroke(CPKT_PDF_Page page) {
  return HPDF_Page_ClosePathEofillStroke(page);
}

/** Ends the current path without painting it; use after clip/eoclip. */
CPKT_PDF_STATUS cpkt_pdf_page_end_path(CPKT_PDF_Page page) {
  return HPDF_Page_EndPath(page);
}

/** Sets a clipping path using the nonzero winding rule; call end_path
 * afterward. */
CPKT_PDF_STATUS cpkt_pdf_page_clip(CPKT_PDF_Page page) {
  return HPDF_Page_Clip(page);
}

/** Sets a clipping path using the even-odd rule; call end_path afterward. */
CPKT_PDF_STATUS cpkt_pdf_page_eoclip(CPKT_PDF_Page page) {
  return HPDF_Page_Eoclip(page);
}

/** Calls libHaru's HPDF_Page_BeginText with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_begin_text(CPKT_PDF_Page page) {
  return HPDF_Page_BeginText(page);
}

/** Calls libHaru's HPDF_Page_EndText with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_end_text(CPKT_PDF_Page page) {
  return HPDF_Page_EndText(page);
}

/** Calls libHaru's HPDF_Page_SetCharSpace with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_char_space(CPKT_PDF_Page page,
                                             CPKT_PDF_REAL value) {
  return HPDF_Page_SetCharSpace(page, value);
}

/** Calls libHaru's HPDF_Page_SetWordSpace with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_word_space(CPKT_PDF_Page page,
                                             CPKT_PDF_REAL value) {
  return HPDF_Page_SetWordSpace(page, value);
}

/** Calls libHaru's HPDF_Page_SetHorizontalScalling with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_horizontal_scalling(CPKT_PDF_Page page,
                                                      CPKT_PDF_REAL value) {
  return HPDF_Page_SetHorizontalScalling(page, value);
}

/** Calls libHaru's HPDF_Page_SetTextLeading with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_text_leading(CPKT_PDF_Page page,
                                               CPKT_PDF_REAL value) {
  return HPDF_Page_SetTextLeading(page, value);
}

/** Calls libHaru's HPDF_Page_SetFontAndSize with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_font_and_size(CPKT_PDF_Page page,
                                                CPKT_PDF_Font font,
                                                CPKT_PDF_REAL size) {
  return HPDF_Page_SetFontAndSize(page, font, size);
}

/** Calls libHaru's HPDF_Page_SetTextRenderingMode with C89 facade types. */
CPKT_PDF_STATUS
cpkt_pdf_page_set_text_rendering_mode(CPKT_PDF_Page page,
                                      CPKT_PDF_TextRenderingMode mode) {
  return HPDF_Page_SetTextRenderingMode(page, (HPDF_TextRenderingMode)mode);
}

/** Calls libHaru's HPDF_Page_SetTextRise with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_text_rise(CPKT_PDF_Page page,
                                            CPKT_PDF_REAL value) {
  return HPDF_Page_SetTextRise(page, value);
}

/** Calls libHaru's HPDF_Page_SetTextRaise with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_text_raise(CPKT_PDF_Page page,
                                             CPKT_PDF_REAL value) {
  return HPDF_Page_SetTextRaise(page, value);
}

/** Calls libHaru's HPDF_Page_MoveTextPos with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_move_text_pos(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                            CPKT_PDF_REAL y) {
  return HPDF_Page_MoveTextPos(page, x, y);
}

/** Calls libHaru's HPDF_Page_MoveTextPos2 with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_move_text_pos2(CPKT_PDF_Page page,
                                             CPKT_PDF_REAL x, CPKT_PDF_REAL y) {
  return HPDF_Page_MoveTextPos2(page, x, y);
}

/** Calls libHaru's HPDF_Page_SetTextMatrix with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_text_matrix(CPKT_PDF_Page page,
                                              CPKT_PDF_REAL a, CPKT_PDF_REAL b,
                                              CPKT_PDF_REAL c, CPKT_PDF_REAL d,
                                              CPKT_PDF_REAL x,
                                              CPKT_PDF_REAL y) {
  return HPDF_Page_SetTextMatrix(page, a, b, c, d, x, y);
}

/** Calls libHaru's HPDF_Page_MoveToNextLine with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_move_to_next_line(CPKT_PDF_Page page) {
  return HPDF_Page_MoveToNextLine(page);
}

/** Calls libHaru's HPDF_Page_ShowText with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_show_text(CPKT_PDF_Page page, const char *text) {
  return HPDF_Page_ShowText(page, text);
}

/** Calls libHaru's HPDF_Page_ShowTextNextLine with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_show_text_next_line(CPKT_PDF_Page page,
                                                  const char *text) {
  return HPDF_Page_ShowTextNextLine(page, text);
}

/** Calls libHaru's HPDF_Page_ShowTextNextLineEx with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_show_text_next_line_ex(CPKT_PDF_Page page,
                                                     CPKT_PDF_REAL word_space,
                                                     CPKT_PDF_REAL char_space,
                                                     const char *text) {
  return HPDF_Page_ShowTextNextLineEx(page, word_space, char_space, text);
}

/** Calls libHaru's HPDF_Page_SetGrayFill with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_gray_fill(CPKT_PDF_Page page,
                                            CPKT_PDF_REAL gray) {
  return HPDF_Page_SetGrayFill(page, gray);
}

/** Calls libHaru's HPDF_Page_SetGrayStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_gray_stroke(CPKT_PDF_Page page,
                                              CPKT_PDF_REAL gray) {
  return HPDF_Page_SetGrayStroke(page, gray);
}

/** Sets the page's RGB fill color; each component is in the range 0 to 1. */
CPKT_PDF_STATUS cpkt_pdf_page_set_rgb_fill(CPKT_PDF_Page page, CPKT_PDF_REAL r,
                                           CPKT_PDF_REAL g, CPKT_PDF_REAL b) {
  return HPDF_Page_SetRGBFill(page, r, g, b);
}

/** Sets the page's RGB stroke color; each component is in the range 0 to 1. */
CPKT_PDF_STATUS cpkt_pdf_page_set_rgb_stroke(CPKT_PDF_Page page,
                                             CPKT_PDF_REAL r, CPKT_PDF_REAL g,
                                             CPKT_PDF_REAL b) {
  return HPDF_Page_SetRGBStroke(page, r, g, b);
}

/** Calls libHaru's HPDF_Page_SetCMYKFill with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_cmyk_fill(CPKT_PDF_Page page, CPKT_PDF_REAL c,
                                            CPKT_PDF_REAL m, CPKT_PDF_REAL y,
                                            CPKT_PDF_REAL k) {
  return HPDF_Page_SetCMYKFill(page, c, m, y, k);
}

/** Calls libHaru's HPDF_Page_SetCMYKStroke with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_cmyk_stroke(CPKT_PDF_Page page,
                                              CPKT_PDF_REAL c, CPKT_PDF_REAL m,
                                              CPKT_PDF_REAL y,
                                              CPKT_PDF_REAL k) {
  return HPDF_Page_SetCMYKStroke(page, c, m, y, k);
}

/** Calls libHaru's HPDF_Shading_New with C89 facade types. */
CPKT_PDF_Shading cpkt_pdf_shading_new(CPKT_PDF_Doc pdf,
                                      CPKT_PDF_ShadingType type,
                                      CPKT_PDF_ColorSpace colorSpace,
                                      CPKT_PDF_REAL xMin, CPKT_PDF_REAL xMax,
                                      CPKT_PDF_REAL yMin, CPKT_PDF_REAL yMax) {
  return HPDF_Shading_New(pdf, (HPDF_ShadingType)type,
                          (HPDF_ColorSpace)colorSpace, xMin, xMax, yMin, yMax);
}

/** Calls libHaru's HPDF_Shading_AddVertexRGB with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_shading_add_vertex_rgb(
    CPKT_PDF_Shading shading,
    CPKT_PDF_Shading_FreeFormTriangleMeshEdgeFlag edgeFlag, CPKT_PDF_REAL x,
    CPKT_PDF_REAL y, CPKT_PDF_UINT8 r, CPKT_PDF_UINT8 g, CPKT_PDF_UINT8 b) {
  return HPDF_Shading_AddVertexRGB(
      shading, (HPDF_Shading_FreeFormTriangleMeshEdgeFlag)edgeFlag, x, y, r, g,
      b);
}

/** Calls libHaru's HPDF_Page_ExecuteXObject with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_execute_x_object(CPKT_PDF_Page page,
                                               CPKT_PDF_XObject obj) {
  return HPDF_Page_ExecuteXObject(page, obj);
}

/** Calls libHaru's HPDF_Page_New_Content_Stream with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_new_content_stream(CPKT_PDF_Page page,
                                                 CPKT_PDF_Dict *new_stream) {
  return HPDF_Page_New_Content_Stream(page, new_stream);
}

/** Calls libHaru's HPDF_Page_Insert_Shared_Content_Stream with C89 facade
 * types. */
CPKT_PDF_STATUS
cpkt_pdf_page_insert_shared_content_stream(CPKT_PDF_Page page,
                                           CPKT_PDF_Dict shared_stream) {
  return HPDF_Page_Insert_Shared_Content_Stream(page, shared_stream);
}

/** Draws a document-owned image at the given page position and dimensions. */
CPKT_PDF_STATUS cpkt_pdf_page_draw_image(CPKT_PDF_Page page,
                                         CPKT_PDF_Image image, CPKT_PDF_REAL x,
                                         CPKT_PDF_REAL y, CPKT_PDF_REAL width,
                                         CPKT_PDF_REAL height) {
  return HPDF_Page_DrawImage(page, image, x, y, width, height);
}

/** Calls libHaru's HPDF_Page_Circle with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_circle(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                     CPKT_PDF_REAL y, CPKT_PDF_REAL ray) {
  return HPDF_Page_Circle(page, x, y, ray);
}

/** Adds an ellipse path centered at the given page coordinates. */
CPKT_PDF_STATUS cpkt_pdf_page_ellipse(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                      CPKT_PDF_REAL y, CPKT_PDF_REAL xray,
                                      CPKT_PDF_REAL yray) {
  return HPDF_Page_Ellipse(page, x, y, xray, yray);
}

/** Adds an arc path centered at the given page coordinates. */
CPKT_PDF_STATUS cpkt_pdf_page_arc(CPKT_PDF_Page page, CPKT_PDF_REAL x,
                                  CPKT_PDF_REAL y, CPKT_PDF_REAL ray,
                                  CPKT_PDF_REAL ang1, CPKT_PDF_REAL ang2) {
  return HPDF_Page_Arc(page, x, y, ray, ang1, ang2);
}

/** Calls libHaru's HPDF_Page_TextOut with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_text_out(CPKT_PDF_Page page, CPKT_PDF_REAL xpos,
                                       CPKT_PDF_REAL ypos, const char *text) {
  return HPDF_Page_TextOut(page, xpos, ypos, text);
}

/** Calls libHaru's HPDF_Page_TextRect with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_text_rect(CPKT_PDF_Page page, CPKT_PDF_REAL left,
                                        CPKT_PDF_REAL top, CPKT_PDF_REAL right,
                                        CPKT_PDF_REAL bottom, const char *text,
                                        CPKT_PDF_TextAlignment align,
                                        CPKT_PDF_UINT *len) {
  return HPDF_Page_TextRect(page, left, top, right, bottom, text,
                            (HPDF_TextAlignment)align, len);
}

/** Calls libHaru's HPDF_Page_SetSlideShow with C89 facade types. */
CPKT_PDF_STATUS cpkt_pdf_page_set_slide_show(CPKT_PDF_Page page,
                                             CPKT_PDF_TransitionStyle type,
                                             CPKT_PDF_REAL disp_time,
                                             CPKT_PDF_REAL trans_time) {
  return HPDF_Page_SetSlideShow(page, (HPDF_TransitionStyle)type, disp_time,
                                trans_time);
}

/** Calls libHaru's HPDF_ICC_LoadIccFromMem with C89 facade types. */
CPKT_PDF_OutputIntent cpkt_pdf_icc_load_icc_from_mem(CPKT_PDF_Doc pdf,
                                                     CPKT_PDF_MMgr mmgr,
                                                     CPKT_PDF_Stream iccdata,
                                                     CPKT_PDF_Xref xref,
                                                     int numcomponent) {
  return HPDF_ICC_LoadIccFromMem(pdf, mmgr, iccdata, xref, numcomponent);
}

/** Calls libHaru's HPDF_LoadIccProfileFromFile with C89 facade types. */
CPKT_PDF_OutputIntent
cpkt_pdf_load_icc_profile_from_file(CPKT_PDF_Doc pdf, const char *icc_file_name,
                                    int numcomponent) {
  return HPDF_LoadIccProfileFromFile(pdf, icc_file_name, numcomponent);
}
