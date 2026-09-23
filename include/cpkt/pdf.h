/* Generated from libHaru 2.4.6 public headers; see
 * tools/generate_pdf_facade.py. */
/* libHaru license: share/doc/c.pkt.systems/third_party/libharu/LICENSE. */
#ifndef CPKT_PDF_H
#define CPKT_PDF_H
#include <stdlib.h>
#if defined(__GNUC__) || defined(__clang__)
#define CPKT_PDF_API __attribute__((visibility("default")))
#else
#define CPKT_PDF_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define CPKT_PDF_STDCALL

/** @defgroup cpkt_pdf libHaru C89 PDF facade
 * Documents own their pages, fonts, and images. PDF stream output is
 * buffered in memory; read_from_stream consumes that saved buffer.
 * See docs/pdf-c89-facade.md for ownership and output behavior.
 * @{ */

typedef signed int CPKT_PDF_INT;
typedef unsigned int CPKT_PDF_UINT;

typedef struct {
  unsigned int lo;
  signed int hi;
} CPKT_PDF_INT64;
typedef struct {
  unsigned int lo;
  unsigned int hi;
} CPKT_PDF_UINT64;

typedef signed int CPKT_PDF_INT32;
typedef unsigned int CPKT_PDF_UINT32;

typedef signed short CPKT_PDF_INT16;
typedef unsigned short CPKT_PDF_UINT16;

typedef signed char CPKT_PDF_INT8;
typedef unsigned char CPKT_PDF_UINT8;

typedef unsigned char CPKT_PDF_BYTE;

typedef float CPKT_PDF_REAL;

typedef double CPKT_PDF_DOUBLE;

typedef signed int CPKT_PDF_BOOL;

typedef unsigned long CPKT_PDF_STATUS;

typedef CPKT_PDF_UINT16 CPKT_PDF_CID;
typedef CPKT_PDF_UINT16 CPKT_PDF_UNICODE;

typedef struct _CPKT_PDF_Point {
  CPKT_PDF_REAL x;
  CPKT_PDF_REAL y;
} CPKT_PDF_Point;

typedef struct _CPKT_PDF_Rect {
  CPKT_PDF_REAL left;
  CPKT_PDF_REAL bottom;
  CPKT_PDF_REAL right;
  CPKT_PDF_REAL top;
} CPKT_PDF_Rect;

typedef struct _CPKT_PDF_Point3D {
  CPKT_PDF_REAL x;
  CPKT_PDF_REAL y;
  CPKT_PDF_REAL z;
} CPKT_PDF_Point3D;

typedef struct _CPKT_PDF_Rect CPKT_PDF_Box;

typedef struct _CPKT_PDF_Date {
  CPKT_PDF_INT year;
  CPKT_PDF_INT month;
  CPKT_PDF_INT day;
  CPKT_PDF_INT hour;
  CPKT_PDF_INT minutes;
  CPKT_PDF_INT seconds;
  char ind;
  CPKT_PDF_INT off_hour;
  CPKT_PDF_INT off_minutes;
} CPKT_PDF_Date;

typedef enum _CPKT_PDF_InfoType {

  CPKT_PDF_INFO_CREATION_DATE = 0,
  CPKT_PDF_INFO_MOD_DATE,

  CPKT_PDF_INFO_AUTHOR,
  CPKT_PDF_INFO_CREATOR,
  CPKT_PDF_INFO_PRODUCER,
  CPKT_PDF_INFO_TITLE,
  CPKT_PDF_INFO_SUBJECT,
  CPKT_PDF_INFO_KEYWORDS,
  CPKT_PDF_INFO_TRAPPED,
  CPKT_PDF_INFO_GTS_PDFX,
  CPKT_PDF_INFO_EOF
} CPKT_PDF_InfoType;

typedef enum _CPKT_PDF_PDFA_TYPE {
  CPKT_PDF_PDFA_NON_PDFA = -1,

  CPKT_PDF_PDFA_1A = 0,
  CPKT_PDF_PDFA_1B,
  CPKT_PDF_PDFA_2A,
  CPKT_PDF_PDFA_2B,
  CPKT_PDF_PDFA_2U,
  CPKT_PDF_PDFA_3A,
  CPKT_PDF_PDFA_3B,
  CPKT_PDF_PDFA_3U,
  CPKT_PDF_PDFA_4,
  CPKT_PDF_PDFA_4E,
  CPKT_PDF_PDFA_4F
} CPKT_PDF_PDFAType;

typedef enum _CPKT_PDF_PdfVer {
  CPKT_PDF_VER_12 = 0,
  CPKT_PDF_VER_13,
  CPKT_PDF_VER_14,
  CPKT_PDF_VER_15,
  CPKT_PDF_VER_16,
  CPKT_PDF_VER_17,
  CPKT_PDF_VER_20,
  CPKT_PDF_VER_EOF
} CPKT_PDF_PDFVer;

typedef enum _CPKT_PDF_EncryptMode {
  CPKT_PDF_ENCRYPT_R2 = 2,
  CPKT_PDF_ENCRYPT_R3 = 3
} CPKT_PDF_EncryptMode;

typedef void(CPKT_PDF_STDCALL *CPKT_PDF_Error_Handler)(
    CPKT_PDF_STATUS error_no, CPKT_PDF_STATUS detail_no, void *user_data);

typedef void *(CPKT_PDF_STDCALL *CPKT_PDF_Alloc_Func)(CPKT_PDF_UINT size);

typedef void(CPKT_PDF_STDCALL *CPKT_PDF_Free_Func)(void *aptr);

typedef struct _CPKT_PDF_TextWidth {
  CPKT_PDF_UINT numchars;

  CPKT_PDF_UINT numwords;

  CPKT_PDF_UINT width;
  CPKT_PDF_UINT numspace;
} CPKT_PDF_TextWidth;

typedef struct _CPKT_PDF_DashMode {
  CPKT_PDF_REAL ptn[8];
  CPKT_PDF_UINT num_ptn;
  CPKT_PDF_REAL phase;
} CPKT_PDF_DashMode;

typedef struct _CPKT_PDF_TransMatrix {
  CPKT_PDF_REAL a;
  CPKT_PDF_REAL b;
  CPKT_PDF_REAL c;
  CPKT_PDF_REAL d;
  CPKT_PDF_REAL x;
  CPKT_PDF_REAL y;
} CPKT_PDF_TransMatrix;

typedef struct _CPKT_PDF_3DMatrix {
  CPKT_PDF_REAL a;
  CPKT_PDF_REAL b;
  CPKT_PDF_REAL c;
  CPKT_PDF_REAL d;
  CPKT_PDF_REAL e;
  CPKT_PDF_REAL f;
  CPKT_PDF_REAL g;
  CPKT_PDF_REAL h;
  CPKT_PDF_REAL i;
  CPKT_PDF_REAL tx;
  CPKT_PDF_REAL ty;
  CPKT_PDF_REAL tz;
} CPKT_PDF_3DMatrix;

typedef enum _CPKT_PDF_ColorSpace {
  CPKT_PDF_CS_DEVICE_GRAY = 0,
  CPKT_PDF_CS_DEVICE_RGB,
  CPKT_PDF_CS_DEVICE_CMYK,
  CPKT_PDF_CS_CAL_GRAY,
  CPKT_PDF_CS_CAL_RGB,
  CPKT_PDF_CS_LAB,
  CPKT_PDF_CS_ICC_BASED,
  CPKT_PDF_CS_SEPARATION,
  CPKT_PDF_CS_DEVICE_N,
  CPKT_PDF_CS_INDEXED,
  CPKT_PDF_CS_PATTERN,
  CPKT_PDF_CS_EOF
} CPKT_PDF_ColorSpace;

typedef struct _CPKT_PDF_RGBColor {
  CPKT_PDF_REAL r;
  CPKT_PDF_REAL g;
  CPKT_PDF_REAL b;
} CPKT_PDF_RGBColor;

typedef struct _CPKT_PDF_CMYKColor {
  CPKT_PDF_REAL c;
  CPKT_PDF_REAL m;
  CPKT_PDF_REAL y;
  CPKT_PDF_REAL k;
} CPKT_PDF_CMYKColor;

typedef enum _CPKT_PDF_LineCap {
  CPKT_PDF_BUTT_END = 0,
  CPKT_PDF_ROUND_END,
  CPKT_PDF_PROJECTING_SQUARE_END,
  CPKT_PDF_LINECAP_EOF
} CPKT_PDF_LineCap;

typedef enum _CPKT_PDF_LineJoin {
  CPKT_PDF_MITER_JOIN = 0,
  CPKT_PDF_ROUND_JOIN,
  CPKT_PDF_BEVEL_JOIN,
  CPKT_PDF_LINEJOIN_EOF
} CPKT_PDF_LineJoin;

typedef enum _CPKT_PDF_TextRenderingMode {
  CPKT_PDF_FILL = 0,
  CPKT_PDF_STROKE,
  CPKT_PDF_FILL_THEN_STROKE,
  CPKT_PDF_INVISIBLE,
  CPKT_PDF_FILL_CLIPPING,
  CPKT_PDF_STROKE_CLIPPING,
  CPKT_PDF_FILL_STROKE_CLIPPING,
  CPKT_PDF_CLIPPING,
  CPKT_PDF_RENDERING_MODE_EOF
} CPKT_PDF_TextRenderingMode;

typedef enum _CPKT_PDF_WritingMode {
  CPKT_PDF_WMODE_HORIZONTAL = 0,
  CPKT_PDF_WMODE_VERTICAL,
  CPKT_PDF_WMODE_EOF
} CPKT_PDF_WritingMode;

typedef enum _CPKT_PDF_PageLayout {
  CPKT_PDF_PAGE_LAYOUT_SINGLE = 0,
  CPKT_PDF_PAGE_LAYOUT_ONE_COLUMN,
  CPKT_PDF_PAGE_LAYOUT_TWO_COLUMN_LEFT,
  CPKT_PDF_PAGE_LAYOUT_TWO_COLUMN_RIGHT,
  CPKT_PDF_PAGE_LAYOUT_TWO_PAGE_LEFT,
  CPKT_PDF_PAGE_LAYOUT_TWO_PAGE_RIGHT,
  CPKT_PDF_PAGE_LAYOUT_EOF
} CPKT_PDF_PageLayout;

typedef enum _CPKT_PDF_PageMode {
  CPKT_PDF_PAGE_MODE_USE_NONE = 0,
  CPKT_PDF_PAGE_MODE_USE_OUTLINE,
  CPKT_PDF_PAGE_MODE_USE_THUMBS,
  CPKT_PDF_PAGE_MODE_FULL_SCREEN,

  CPKT_PDF_PAGE_MODE_EOF
} CPKT_PDF_PageMode;

typedef enum _CPKT_PDF_PageNumStyle {
  CPKT_PDF_PAGE_NUM_STYLE_DECIMAL = 0,
  CPKT_PDF_PAGE_NUM_STYLE_UPPER_ROMAN,
  CPKT_PDF_PAGE_NUM_STYLE_LOWER_ROMAN,
  CPKT_PDF_PAGE_NUM_STYLE_UPPER_LETTERS,
  CPKT_PDF_PAGE_NUM_STYLE_LOWER_LETTERS,
  CPKT_PDF_PAGE_NUM_STYLE_EOF
} CPKT_PDF_PageNumStyle;

typedef enum _CPKT_PDF_DestinationType {
  CPKT_PDF_XYZ = 0,
  CPKT_PDF_FIT,
  CPKT_PDF_FIT_H,
  CPKT_PDF_FIT_V,
  CPKT_PDF_FIT_R,
  CPKT_PDF_FIT_B,
  CPKT_PDF_FIT_BH,
  CPKT_PDF_FIT_BV,
  CPKT_PDF_DST_EOF
} CPKT_PDF_DestinationType;

typedef enum _CPKT_PDF_AnnotType {
  CPKT_PDF_ANNOT_TEXT_NOTES,
  CPKT_PDF_ANNOT_LINK,
  CPKT_PDF_ANNOT_SOUND,
  CPKT_PDF_ANNOT_FREE_TEXT,
  CPKT_PDF_ANNOT_STAMP,
  CPKT_PDF_ANNOT_SQUARE,
  CPKT_PDF_ANNOT_CIRCLE,
  CPKT_PDF_ANNOT_STRIKE_OUT,
  CPKT_PDF_ANNOT_HIGHTLIGHT,
  CPKT_PDF_ANNOT_UNDERLINE,
  CPKT_PDF_ANNOT_INK,
  CPKT_PDF_ANNOT_FILE_ATTACHMENT,
  CPKT_PDF_ANNOT_POPUP,
  CPKT_PDF_ANNOT_3D,
  CPKT_PDF_ANNOT_SQUIGGLY,
  CPKT_PDF_ANNOT_LINE,
  CPKT_PDF_ANNOT_PROJECTION,
  CPKT_PDF_ANNOT_WIDGET
} CPKT_PDF_AnnotType;

typedef enum _CPKT_PDF_AnnotFlgs {
  CPKT_PDF_ANNOT_INVISIBLE,
  CPKT_PDF_ANNOT_HIDDEN,
  CPKT_PDF_ANNOT_PRINT,
  CPKT_PDF_ANNOT_NOZOOM,
  CPKT_PDF_ANNOT_NOROTATE,
  CPKT_PDF_ANNOT_NOVIEW,
  CPKT_PDF_ANNOT_READONLY
} CPKT_PDF_AnnotFlgs;

typedef enum _CPKT_PDF_AnnotHighlightMode {
  CPKT_PDF_ANNOT_NO_HIGHTLIGHT = 0,
  CPKT_PDF_ANNOT_INVERT_BOX,
  CPKT_PDF_ANNOT_INVERT_BORDER,
  CPKT_PDF_ANNOT_DOWN_APPEARANCE,
  CPKT_PDF_ANNOT_HIGHTLIGHT_MODE_EOF
} CPKT_PDF_AnnotHighlightMode;

typedef enum _CPKT_PDF_AnnotIcon {
  CPKT_PDF_ANNOT_ICON_COMMENT = 0,
  CPKT_PDF_ANNOT_ICON_KEY,
  CPKT_PDF_ANNOT_ICON_NOTE,
  CPKT_PDF_ANNOT_ICON_HELP,
  CPKT_PDF_ANNOT_ICON_NEW_PARAGRAPH,
  CPKT_PDF_ANNOT_ICON_PARAGRAPH,
  CPKT_PDF_ANNOT_ICON_INSERT,
  CPKT_PDF_ANNOT_ICON_EOF
} CPKT_PDF_AnnotIcon;

typedef enum _CPKT_PDF_AnnotIntent {
  CPKT_PDF_ANNOT_INTENT_FREETEXTCALLOUT = 0,
  CPKT_PDF_ANNOT_INTENT_FREETEXTTYPEWRITER,
  CPKT_PDF_ANNOT_INTENT_LINEARROW,
  CPKT_PDF_ANNOT_INTENT_LINEDIMENSION,
  CPKT_PDF_ANNOT_INTENT_POLYGONCLOUD,
  CPKT_PDF_ANNOT_INTENT_POLYLINEDIMENSION,
  CPKT_PDF_ANNOT_INTENT_POLYGONDIMENSION
} CPKT_PDF_AnnotIntent;

typedef enum _CPKT_PDF_LineAnnotEndingStyle {
  CPKT_PDF_LINE_ANNOT_NONE = 0,
  CPKT_PDF_LINE_ANNOT_SQUARE,
  CPKT_PDF_LINE_ANNOT_CIRCLE,
  CPKT_PDF_LINE_ANNOT_DIAMOND,
  CPKT_PDF_LINE_ANNOT_OPENARROW,
  CPKT_PDF_LINE_ANNOT_CLOSEDARROW,
  CPKT_PDF_LINE_ANNOT_BUTT,
  CPKT_PDF_LINE_ANNOT_ROPENARROW,
  CPKT_PDF_LINE_ANNOT_RCLOSEDARROW,
  CPKT_PDF_LINE_ANNOT_SLASH
} CPKT_PDF_LineAnnotEndingStyle;

typedef enum _CPKT_PDF_LineAnnotCapPosition {
  CPKT_PDF_LINE_ANNOT_CAP_INLINE = 0,
  CPKT_PDF_LINE_ANNOT_CAP_TOP
} CPKT_PDF_LineAnnotCapPosition;

typedef enum _CPKT_PDF_StampAnnotName {
  CPKT_PDF_STAMP_ANNOT_APPROVED = 0,
  CPKT_PDF_STAMP_ANNOT_EXPERIMENTAL,
  CPKT_PDF_STAMP_ANNOT_NOTAPPROVED,
  CPKT_PDF_STAMP_ANNOT_ASIS,
  CPKT_PDF_STAMP_ANNOT_EXPIRED,
  CPKT_PDF_STAMP_ANNOT_NOTFORPUBLICRELEASE,
  CPKT_PDF_STAMP_ANNOT_CONFIDENTIAL,
  CPKT_PDF_STAMP_ANNOT_FINAL,
  CPKT_PDF_STAMP_ANNOT_SOLD,
  CPKT_PDF_STAMP_ANNOT_DEPARTMENTAL,
  CPKT_PDF_STAMP_ANNOT_FORCOMMENT,
  CPKT_PDF_STAMP_ANNOT_TOPSECRET,
  CPKT_PDF_STAMP_ANNOT_DRAFT,
  CPKT_PDF_STAMP_ANNOT_FORPUBLICRELEASE
} CPKT_PDF_StampAnnotName;

typedef enum _CPKT_PDF_BSSubtype {
  CPKT_PDF_BS_SOLID,
  CPKT_PDF_BS_DASHED,
  CPKT_PDF_BS_BEVELED,
  CPKT_PDF_BS_INSET,
  CPKT_PDF_BS_UNDERLINED
} CPKT_PDF_BSSubtype;

typedef enum _CPKT_PDF_BlendMode {
  CPKT_PDF_BM_NORMAL,
  CPKT_PDF_BM_MULTIPLY,
  CPKT_PDF_BM_SCREEN,
  CPKT_PDF_BM_OVERLAY,
  CPKT_PDF_BM_DARKEN,
  CPKT_PDF_BM_LIGHTEN,
  CPKT_PDF_BM_COLOR_DODGE,
  CPKT_PDF_BM_COLOR_BUM,
  CPKT_PDF_BM_HARD_LIGHT,
  CPKT_PDF_BM_SOFT_LIGHT,
  CPKT_PDF_BM_DIFFERENCE,
  CPKT_PDF_BM_EXCLUSHON,
  CPKT_PDF_BM_EOF
} CPKT_PDF_BlendMode;

typedef enum _CPKT_PDF_TransitionStyle {
  CPKT_PDF_TS_WIPE_RIGHT = 0,
  CPKT_PDF_TS_WIPE_UP,
  CPKT_PDF_TS_WIPE_LEFT,
  CPKT_PDF_TS_WIPE_DOWN,
  CPKT_PDF_TS_BARN_DOORS_HORIZONTAL_OUT,
  CPKT_PDF_TS_BARN_DOORS_HORIZONTAL_IN,
  CPKT_PDF_TS_BARN_DOORS_VERTICAL_OUT,
  CPKT_PDF_TS_BARN_DOORS_VERTICAL_IN,
  CPKT_PDF_TS_BOX_OUT,
  CPKT_PDF_TS_BOX_IN,
  CPKT_PDF_TS_BLINDS_HORIZONTAL,
  CPKT_PDF_TS_BLINDS_VERTICAL,
  CPKT_PDF_TS_DISSOLVE,
  CPKT_PDF_TS_GLITTER_RIGHT,
  CPKT_PDF_TS_GLITTER_DOWN,
  CPKT_PDF_TS_GLITTER_TOP_LEFT_TO_BOTTOM_RIGHT,
  CPKT_PDF_TS_REPLACE,
  CPKT_PDF_TS_EOF
} CPKT_PDF_TransitionStyle;

typedef enum _CPKT_PDF_PageSizes {
  CPKT_PDF_PAGE_SIZE_LETTER = 0,
  CPKT_PDF_PAGE_SIZE_LEGAL,
  CPKT_PDF_PAGE_SIZE_A3,
  CPKT_PDF_PAGE_SIZE_A4,
  CPKT_PDF_PAGE_SIZE_A5,
  CPKT_PDF_PAGE_SIZE_B4,
  CPKT_PDF_PAGE_SIZE_B5,
  CPKT_PDF_PAGE_SIZE_EXECUTIVE,
  CPKT_PDF_PAGE_SIZE_US4x6,
  CPKT_PDF_PAGE_SIZE_US4x8,
  CPKT_PDF_PAGE_SIZE_US5x7,
  CPKT_PDF_PAGE_SIZE_COMM10,
  CPKT_PDF_PAGE_SIZE_EOF
} CPKT_PDF_PageSizes;

typedef enum _CPKT_PDF_PageDirection {
  CPKT_PDF_PAGE_PORTRAIT = 0,
  CPKT_PDF_PAGE_LANDSCAPE
} CPKT_PDF_PageDirection;

typedef enum _CPKT_PDF_EncoderType {
  CPKT_PDF_ENCODER_TYPE_SINGLE_BYTE,
  CPKT_PDF_ENCODER_TYPE_DOUBLE_BYTE,
  CPKT_PDF_ENCODER_TYPE_UNINITIALIZED,
  CPKT_PDF_ENCODER_UNKNOWN
} CPKT_PDF_EncoderType;

typedef enum _CPKT_PDF_ByteType {
  CPKT_PDF_BYTE_TYPE_SINGLE = 0,
  CPKT_PDF_BYTE_TYPE_LEAD,
  CPKT_PDF_BYTE_TYPE_TRAIL,
  CPKT_PDF_BYTE_TYPE_UNKNOWN
} CPKT_PDF_ByteType;

typedef enum _CPKT_PDF_TextAlignment {
  CPKT_PDF_TALIGN_LEFT = 0,
  CPKT_PDF_TALIGN_RIGHT,
  CPKT_PDF_TALIGN_CENTER,
  CPKT_PDF_TALIGN_JUSTIFY
} CPKT_PDF_TextAlignment;

typedef enum _CPKT_PDF_NameDictKey {
  CPKT_PDF_NAME_EMBEDDED_FILES = 0,
  CPKT_PDF_NAME_EOF
} CPKT_PDF_NameDictKey;

typedef enum _CPKT_PDF_AFRelationship {
  CPKT_PDF_AFRELATIONSHIP_SOURCE = 0,
  CPKT_PDF_AFRELATIONSHIP_DATA,
  CPKT_PDF_AFRELATIONSHIP_ALTERNATIVE,
  CPKT_PDF_AFRELATIONSHIP_SUPPLEMENT,
  CPKT_PDF_AFRELATIONSHIP_ENCRYPTEDPAYLOAD,
  CPKT_PDF_AFRELATIONSHIP_FORMDATA,
  CPKT_PDF_AFRELATIONSHIP_SCHEMA,
  CPKT_PDF_AFRELATIONSHIP_UNSPECIFIED
} CPKT_PDF_AFRelationship;

typedef enum _CPKT_PDF_PageBoundary {
  CPKT_PDF_PAGE_MEDIABOX = 0,
  CPKT_PDF_PAGE_CROPBOX,
  CPKT_PDF_PAGE_BLEEDBOX,
  CPKT_PDF_PAGE_TRIMBOX,
  CPKT_PDF_PAGE_ARTBOX
} CPKT_PDF_PageBoundary;

typedef enum _CPKT_PDF_ShadingType {
  CPKT_PDF_SHADING_FREE_FORM_TRIANGLE_MESH = 4
} CPKT_PDF_ShadingType;

typedef enum _CPKT_PDF_Shading_FreeFormTriangleMeshEdgeFlag {
  CPKT_PDF_FREE_FORM_TRI_MESH_EDGEFLAG_NO_CONNECTION = 0,
  CPKT_PDF_FREE_FORM_TRI_MESH_EDGEFLAG_BC,
  CPKT_PDF_FREE_FORM_TRI_MESH_EDGEFLAG_AC
} CPKT_PDF_Shading_FreeFormTriangleMeshEdgeFlag;

#define CPKT_PDF_TRUE 1
#define CPKT_PDF_FALSE 0

#define CPKT_PDF_OK 0
#define CPKT_PDF_NOERROR 0

#define CPKT_PDF_TMP_BUF_SIZ 512
#define CPKT_PDF_SHORT_BUF_SIZ 32
#define CPKT_PDF_REAL_LEN 64
#define CPKT_PDF_INT_LEN 11
#define CPKT_PDF_TEXT_DEFAULT_LEN 256
#define CPKT_PDF_UNICODE_HEADER_LEN 2
#define CPKT_PDF_DATE_TIME_STR_LEN 23

#define CPKT_PDF_BYTE_OFFSET_LEN 10
#define CPKT_PDF_OBJ_ID_LEN 7
#define CPKT_PDF_GEN_NO_LEN 5

#define CPKT_PDF_DEF_FONT "Helvetica"
#define CPKT_PDF_DEF_PAGE_LAYOUT CPKT_PDF_PAGE_LAYOUT_SINGLE
#define CPKT_PDF_DEF_PAGE_MODE CPKT_PDF_PAGE_MODE_USE_NONE
#define CPKT_PDF_DEF_WORDSPACE 0
#define CPKT_PDF_DEF_CHARSPACE 0
#define CPKT_PDF_DEF_FONTSIZE 10
#define CPKT_PDF_DEF_HSCALING 100
#define CPKT_PDF_DEF_LEADING 0
#define CPKT_PDF_DEF_RENDERING_MODE CPKT_PDF_FILL
#define CPKT_PDF_DEF_RISE 0
#define CPKT_PDF_DEF_RAISE CPKT_PDF_DEF_RISE
#define CPKT_PDF_DEF_LINEWIDTH 1
#define CPKT_PDF_DEF_LINECAP CPKT_PDF_BUTT_END
#define CPKT_PDF_DEF_LINEJOIN CPKT_PDF_MITER_JOIN
#define CPKT_PDF_DEF_MITERLIMIT 10
#define CPKT_PDF_DEF_FLATNESS 1
#define CPKT_PDF_DEF_PAGE_NUM 1

#define CPKT_PDF_BS_DEF_WIDTH 1

#define CPKT_PDF_DEF_PAGE_WIDTH 595.276F
#define CPKT_PDF_DEF_PAGE_HEIGHT 841.89F

#define CPKT_PDF_COMP_NONE 0x00
#define CPKT_PDF_COMP_TEXT 0x01
#define CPKT_PDF_COMP_IMAGE 0x02
#define CPKT_PDF_COMP_METADATA 0x04
#define CPKT_PDF_COMP_ALL 0x0F

#define CPKT_PDF_COMP_MASK 0xFF

#define CPKT_PDF_ENABLE_READ 0
#define CPKT_PDF_ENABLE_PRINT 4
#define CPKT_PDF_ENABLE_EDIT_ALL 8
#define CPKT_PDF_ENABLE_COPY 16
#define CPKT_PDF_ENABLE_EDIT 32

#define CPKT_PDF_HIDE_TOOLBAR 1
#define CPKT_PDF_HIDE_MENUBAR 2
#define CPKT_PDF_HIDE_WINDOW_UI 4
#define CPKT_PDF_FIT_WINDOW 8
#define CPKT_PDF_CENTER_WINDOW 16
#define CPKT_PDF_PRINT_SCALING_NONE 32

#define CPKT_PDF_LIMIT_MAX_INT 2147483647
#define CPKT_PDF_LIMIT_MIN_INT -2147483647

#define CPKT_PDF_LIMIT_MAX_REAL 3.4E38f
#define CPKT_PDF_LIMIT_MIN_REAL -3.4E38f

#define CPKT_PDF_LIMIT_MAX_STRING_LEN 2147483646
#define CPKT_PDF_LIMIT_MAX_NAME_LEN 127

#define CPKT_PDF_LIMIT_MAX_ARRAY 8388607
#define CPKT_PDF_LIMIT_MAX_DICT_ELEMENT 8388607
#define CPKT_PDF_LIMIT_MAX_XREF_ELEMENT 8388607
#define CPKT_PDF_LIMIT_MAX_GSTATE 28
#define CPKT_PDF_LIMIT_MAX_DEVICE_N 8
#define CPKT_PDF_LIMIT_MAX_DEVICE_N_V15 32
#define CPKT_PDF_LIMIT_MAX_CID 65535
#define CPKT_PDF_MAX_GENERATION_NUM 65535

#define CPKT_PDF_MIN_PAGE_HEIGHT 3
#define CPKT_PDF_MIN_PAGE_WIDTH 3
#define CPKT_PDF_MAX_PAGE_HEIGHT 14400
#define CPKT_PDF_MAX_PAGE_WIDTH 14400
#define CPKT_PDF_MIN_MAGNIFICATION_FACTOR 8
#define CPKT_PDF_MAX_MAGNIFICATION_FACTOR 3200

#define CPKT_PDF_MIN_PAGE_SIZE 3
#define CPKT_PDF_MAX_PAGE_SIZE 14400
#define CPKT_PDF_MIN_HORIZONTALSCALING 10
#define CPKT_PDF_MAX_HORIZONTALSCALING 300
#define CPKT_PDF_MIN_WORDSPACE -30
#define CPKT_PDF_MAX_WORDSPACE 300
#define CPKT_PDF_MIN_CHARSPACE -30
#define CPKT_PDF_MAX_CHARSPACE 300
#define CPKT_PDF_MAX_FONTSIZE 600
#define CPKT_PDF_MAX_ZOOMSIZE 10
#define CPKT_PDF_MAX_LEADING 300
#define CPKT_PDF_MAX_LINEWIDTH 100
#define CPKT_PDF_MAX_DASH_PATTERN 100

#define CPKT_PDF_MAX_JWW_NUM 128

#define CPKT_PDF_COUNTRY_AF "AF"
#define CPKT_PDF_COUNTRY_AL "AL"
#define CPKT_PDF_COUNTRY_DZ "DZ"
#define CPKT_PDF_COUNTRY_AS "AS"
#define CPKT_PDF_COUNTRY_AD "AD"
#define CPKT_PDF_COUNTRY_AO "AO"
#define CPKT_PDF_COUNTRY_AI "AI"
#define CPKT_PDF_COUNTRY_AQ "AQ"
#define CPKT_PDF_COUNTRY_AG "AG"
#define CPKT_PDF_COUNTRY_AR "AR"
#define CPKT_PDF_COUNTRY_AM "AM"
#define CPKT_PDF_COUNTRY_AW "AW"
#define CPKT_PDF_COUNTRY_AU "AU"
#define CPKT_PDF_COUNTRY_AT "AT"
#define CPKT_PDF_COUNTRY_AZ "AZ"
#define CPKT_PDF_COUNTRY_BS "BS"
#define CPKT_PDF_COUNTRY_BH "BH"
#define CPKT_PDF_COUNTRY_BD "BD"
#define CPKT_PDF_COUNTRY_BB "BB"
#define CPKT_PDF_COUNTRY_BY "BY"
#define CPKT_PDF_COUNTRY_BE "BE"
#define CPKT_PDF_COUNTRY_BZ "BZ"
#define CPKT_PDF_COUNTRY_BJ "BJ"
#define CPKT_PDF_COUNTRY_BM "BM"
#define CPKT_PDF_COUNTRY_BT "BT"
#define CPKT_PDF_COUNTRY_BO "BO"
#define CPKT_PDF_COUNTRY_BA "BA"
#define CPKT_PDF_COUNTRY_BW "BW"
#define CPKT_PDF_COUNTRY_BV "BV"
#define CPKT_PDF_COUNTRY_BR "BR"
#define CPKT_PDF_COUNTRY_IO "IO"
#define CPKT_PDF_COUNTRY_BN "BN"
#define CPKT_PDF_COUNTRY_BG "BG"
#define CPKT_PDF_COUNTRY_BF "BF"
#define CPKT_PDF_COUNTRY_BI "BI"
#define CPKT_PDF_COUNTRY_KH "KH"
#define CPKT_PDF_COUNTRY_CM "CM"
#define CPKT_PDF_COUNTRY_CA "CA"
#define CPKT_PDF_COUNTRY_CV "CV"
#define CPKT_PDF_COUNTRY_KY "KY"
#define CPKT_PDF_COUNTRY_CF "CF"
#define CPKT_PDF_COUNTRY_TD "TD"
#define CPKT_PDF_COUNTRY_CL "CL"
#define CPKT_PDF_COUNTRY_CN "CN"
#define CPKT_PDF_COUNTRY_CX "CX"
#define CPKT_PDF_COUNTRY_CC "CC"
#define CPKT_PDF_COUNTRY_CO "CO"
#define CPKT_PDF_COUNTRY_KM "KM"
#define CPKT_PDF_COUNTRY_CG "CG"
#define CPKT_PDF_COUNTRY_CK "CK"
#define CPKT_PDF_COUNTRY_CR "CR"
#define CPKT_PDF_COUNTRY_CI "CI"
#define CPKT_PDF_COUNTRY_HR "HR"
#define CPKT_PDF_COUNTRY_CU "CU"
#define CPKT_PDF_COUNTRY_CY "CY"
#define CPKT_PDF_COUNTRY_CZ "CZ"
#define CPKT_PDF_COUNTRY_DK "DK"
#define CPKT_PDF_COUNTRY_DJ "DJ"
#define CPKT_PDF_COUNTRY_DM "DM"
#define CPKT_PDF_COUNTRY_DO "DO"
#define CPKT_PDF_COUNTRY_TP "TP"
#define CPKT_PDF_COUNTRY_EC "EC"
#define CPKT_PDF_COUNTRY_EG "EG"
#define CPKT_PDF_COUNTRY_SV "SV"
#define CPKT_PDF_COUNTRY_GQ "GQ"
#define CPKT_PDF_COUNTRY_ER "ER"
#define CPKT_PDF_COUNTRY_EE "EE"
#define CPKT_PDF_COUNTRY_ET "ET"
#define CPKT_PDF_COUNTRY_FK "FK"
#define CPKT_PDF_COUNTRY_FO "FO"
#define CPKT_PDF_COUNTRY_FJ "FJ"
#define CPKT_PDF_COUNTRY_FI "FI"
#define CPKT_PDF_COUNTRY_FR "FR"
#define CPKT_PDF_COUNTRY_FX "FX"
#define CPKT_PDF_COUNTRY_GF "GF"
#define CPKT_PDF_COUNTRY_PF "PF"
#define CPKT_PDF_COUNTRY_TF "TF"
#define CPKT_PDF_COUNTRY_GA "GA"
#define CPKT_PDF_COUNTRY_GM "GM"
#define CPKT_PDF_COUNTRY_GE "GE"
#define CPKT_PDF_COUNTRY_DE "DE"
#define CPKT_PDF_COUNTRY_GH "GH"
#define CPKT_PDF_COUNTRY_GI "GI"
#define CPKT_PDF_COUNTRY_GR "GR"
#define CPKT_PDF_COUNTRY_GL "GL"
#define CPKT_PDF_COUNTRY_GD "GD"
#define CPKT_PDF_COUNTRY_GP "GP"
#define CPKT_PDF_COUNTRY_GU "GU"
#define CPKT_PDF_COUNTRY_GT "GT"
#define CPKT_PDF_COUNTRY_GN "GN"
#define CPKT_PDF_COUNTRY_GW "GW"
#define CPKT_PDF_COUNTRY_GY "GY"
#define CPKT_PDF_COUNTRY_HT "HT"
#define CPKT_PDF_COUNTRY_HM "HM"
#define CPKT_PDF_COUNTRY_HN "HN"
#define CPKT_PDF_COUNTRY_HK "HK"
#define CPKT_PDF_COUNTRY_HU "HU"
#define CPKT_PDF_COUNTRY_IS "IS"
#define CPKT_PDF_COUNTRY_IN "IN"
#define CPKT_PDF_COUNTRY_ID "ID"
#define CPKT_PDF_COUNTRY_IR "IR"
#define CPKT_PDF_COUNTRY_IQ "IQ"
#define CPKT_PDF_COUNTRY_IE "IE"
#define CPKT_PDF_COUNTRY_IL "IL"
#define CPKT_PDF_COUNTRY_IT "IT"
#define CPKT_PDF_COUNTRY_JM "JM"
#define CPKT_PDF_COUNTRY_JP "JP"
#define CPKT_PDF_COUNTRY_JO "JO"
#define CPKT_PDF_COUNTRY_KZ "KZ"
#define CPKT_PDF_COUNTRY_KE "KE"
#define CPKT_PDF_COUNTRY_KI "KI"
#define CPKT_PDF_COUNTRY_KP "KP"
#define CPKT_PDF_COUNTRY_KR "KR"
#define CPKT_PDF_COUNTRY_KW "KW"
#define CPKT_PDF_COUNTRY_KG "KG"
#define CPKT_PDF_COUNTRY_LA "LA"
#define CPKT_PDF_COUNTRY_LV "LV"
#define CPKT_PDF_COUNTRY_LB "LB"
#define CPKT_PDF_COUNTRY_LS "LS"
#define CPKT_PDF_COUNTRY_LR "LR"
#define CPKT_PDF_COUNTRY_LY "LY"
#define CPKT_PDF_COUNTRY_LI "LI"
#define CPKT_PDF_COUNTRY_LT "LT"
#define CPKT_PDF_COUNTRY_LU "LU"
#define CPKT_PDF_COUNTRY_MO "MO"
#define CPKT_PDF_COUNTRY_MK "MK"
#define CPKT_PDF_COUNTRY_MG "MG"
#define CPKT_PDF_COUNTRY_MW "MW"
#define CPKT_PDF_COUNTRY_MY "MY"
#define CPKT_PDF_COUNTRY_MV "MV"
#define CPKT_PDF_COUNTRY_ML "ML"
#define CPKT_PDF_COUNTRY_MT "MT"
#define CPKT_PDF_COUNTRY_MH "MH"
#define CPKT_PDF_COUNTRY_MQ "MQ"
#define CPKT_PDF_COUNTRY_MR "MR"
#define CPKT_PDF_COUNTRY_MU "MU"
#define CPKT_PDF_COUNTRY_YT "YT"
#define CPKT_PDF_COUNTRY_MX "MX"
#define CPKT_PDF_COUNTRY_FM "FM"
#define CPKT_PDF_COUNTRY_MD "MD"
#define CPKT_PDF_COUNTRY_MC "MC"
#define CPKT_PDF_COUNTRY_MN "MN"
#define CPKT_PDF_COUNTRY_MS "MS"
#define CPKT_PDF_COUNTRY_MA "MA"
#define CPKT_PDF_COUNTRY_MZ "MZ"
#define CPKT_PDF_COUNTRY_MM "MM"
#define CPKT_PDF_COUNTRY_NA "NA"
#define CPKT_PDF_COUNTRY_NR "NR"
#define CPKT_PDF_COUNTRY_NP "NP"
#define CPKT_PDF_COUNTRY_NL "NL"
#define CPKT_PDF_COUNTRY_AN "AN"
#define CPKT_PDF_COUNTRY_NC "NC"
#define CPKT_PDF_COUNTRY_NZ "NZ"
#define CPKT_PDF_COUNTRY_NI "NI"
#define CPKT_PDF_COUNTRY_NE "NE"
#define CPKT_PDF_COUNTRY_NG "NG"
#define CPKT_PDF_COUNTRY_NU "NU"
#define CPKT_PDF_COUNTRY_NF "NF"
#define CPKT_PDF_COUNTRY_MP "MP"
#define CPKT_PDF_COUNTRY_NO "NO"
#define CPKT_PDF_COUNTRY_OM "OM"
#define CPKT_PDF_COUNTRY_PK "PK"
#define CPKT_PDF_COUNTRY_PW "PW"
#define CPKT_PDF_COUNTRY_PA "PA"
#define CPKT_PDF_COUNTRY_PG "PG"
#define CPKT_PDF_COUNTRY_PY "PY"
#define CPKT_PDF_COUNTRY_PE "PE"
#define CPKT_PDF_COUNTRY_PH "PH"
#define CPKT_PDF_COUNTRY_PN "PN"
#define CPKT_PDF_COUNTRY_PL "PL"
#define CPKT_PDF_COUNTRY_PT "PT"
#define CPKT_PDF_COUNTRY_PR "PR"
#define CPKT_PDF_COUNTRY_QA "QA"
#define CPKT_PDF_COUNTRY_RE "RE"
#define CPKT_PDF_COUNTRY_RO "RO"
#define CPKT_PDF_COUNTRY_RU "RU"
#define CPKT_PDF_COUNTRY_RW "RW"
#define CPKT_PDF_COUNTRY_KN "KN"
#define CPKT_PDF_COUNTRY_LC "LC"
#define CPKT_PDF_COUNTRY_VC "VC"
#define CPKT_PDF_COUNTRY_WS "WS"
#define CPKT_PDF_COUNTRY_SM "SM"
#define CPKT_PDF_COUNTRY_ST "ST"
#define CPKT_PDF_COUNTRY_SA "SA"
#define CPKT_PDF_COUNTRY_SN "SN"
#define CPKT_PDF_COUNTRY_SC "SC"
#define CPKT_PDF_COUNTRY_SL "SL"
#define CPKT_PDF_COUNTRY_SG "SG"
#define CPKT_PDF_COUNTRY_SK "SK"
#define CPKT_PDF_COUNTRY_SI "SI"
#define CPKT_PDF_COUNTRY_SB "SB"
#define CPKT_PDF_COUNTRY_SO "SO"
#define CPKT_PDF_COUNTRY_ZA "ZA"
#define CPKT_PDF_COUNTRY_ES "ES"
#define CPKT_PDF_COUNTRY_LK "LK"
#define CPKT_PDF_COUNTRY_SH "SH"
#define CPKT_PDF_COUNTRY_PM "PM"
#define CPKT_PDF_COUNTRY_SD "SD"
#define CPKT_PDF_COUNTRY_SR "SR"
#define CPKT_PDF_COUNTRY_SJ "SJ"
#define CPKT_PDF_COUNTRY_SZ "SZ"
#define CPKT_PDF_COUNTRY_SE "SE"
#define CPKT_PDF_COUNTRY_CH "CH"
#define CPKT_PDF_COUNTRY_SY "SY"
#define CPKT_PDF_COUNTRY_TW "TW"
#define CPKT_PDF_COUNTRY_TJ "TJ"
#define CPKT_PDF_COUNTRY_TZ "TZ"
#define CPKT_PDF_COUNTRY_TH "TH"
#define CPKT_PDF_COUNTRY_TG "TG"
#define CPKT_PDF_COUNTRY_TK "TK"
#define CPKT_PDF_COUNTRY_TO "TO"
#define CPKT_PDF_COUNTRY_TT "TT"
#define CPKT_PDF_COUNTRY_TN "TN"
#define CPKT_PDF_COUNTRY_TR "TR"
#define CPKT_PDF_COUNTRY_TM "TM"
#define CPKT_PDF_COUNTRY_TC "TC"
#define CPKT_PDF_COUNTRY_TV "TV"
#define CPKT_PDF_COUNTRY_UG "UG"
#define CPKT_PDF_COUNTRY_UA "UA"
#define CPKT_PDF_COUNTRY_AE "AE"
#define CPKT_PDF_COUNTRY_GB "GB"
#define CPKT_PDF_COUNTRY_US "US"
#define CPKT_PDF_COUNTRY_UM "UM"
#define CPKT_PDF_COUNTRY_UY "UY"
#define CPKT_PDF_COUNTRY_UZ "UZ"
#define CPKT_PDF_COUNTRY_VU "VU"
#define CPKT_PDF_COUNTRY_VA "VA"
#define CPKT_PDF_COUNTRY_VE "VE"
#define CPKT_PDF_COUNTRY_VN "VN"
#define CPKT_PDF_COUNTRY_VG "VG"
#define CPKT_PDF_COUNTRY_VI "VI"
#define CPKT_PDF_COUNTRY_WF "WF"
#define CPKT_PDF_COUNTRY_EH "EH"
#define CPKT_PDF_COUNTRY_YE "YE"
#define CPKT_PDF_COUNTRY_YU "YU"
#define CPKT_PDF_COUNTRY_ZR "ZR"
#define CPKT_PDF_COUNTRY_ZM "ZM"
#define CPKT_PDF_COUNTRY_ZW "ZW"

#define CPKT_PDF_LANG_AA "aa"
#define CPKT_PDF_LANG_AB "ab"
#define CPKT_PDF_LANG_AF "af"
#define CPKT_PDF_LANG_AM "am"
#define CPKT_PDF_LANG_AR "ar"
#define CPKT_PDF_LANG_AS "as"
#define CPKT_PDF_LANG_AY "ay"
#define CPKT_PDF_LANG_AZ "az"
#define CPKT_PDF_LANG_BA "ba"
#define CPKT_PDF_LANG_BE "be"
#define CPKT_PDF_LANG_BG "bg"
#define CPKT_PDF_LANG_BH "bh"
#define CPKT_PDF_LANG_BI "bi"
#define CPKT_PDF_LANG_BN "bn"
#define CPKT_PDF_LANG_BO "bo"
#define CPKT_PDF_LANG_BR "br"
#define CPKT_PDF_LANG_CA "ca"
#define CPKT_PDF_LANG_CO "co"
#define CPKT_PDF_LANG_CS "cs"
#define CPKT_PDF_LANG_CY "cy"
#define CPKT_PDF_LANG_DA "da"
#define CPKT_PDF_LANG_DE "de"
#define CPKT_PDF_LANG_DZ "dz"
#define CPKT_PDF_LANG_EL "el"
#define CPKT_PDF_LANG_EN "en"
#define CPKT_PDF_LANG_EO "eo"
#define CPKT_PDF_LANG_ES "es"
#define CPKT_PDF_LANG_ET "et"
#define CPKT_PDF_LANG_EU "eu"
#define CPKT_PDF_LANG_FA "fa"
#define CPKT_PDF_LANG_FI "fi"
#define CPKT_PDF_LANG_FJ "fj"
#define CPKT_PDF_LANG_FO "fo"
#define CPKT_PDF_LANG_FR "fr"
#define CPKT_PDF_LANG_FY "fy"
#define CPKT_PDF_LANG_GA "ga"
#define CPKT_PDF_LANG_GD "gd"
#define CPKT_PDF_LANG_GL "gl"
#define CPKT_PDF_LANG_GN "gn"
#define CPKT_PDF_LANG_GU "gu"
#define CPKT_PDF_LANG_HA "ha"
#define CPKT_PDF_LANG_HI "hi"
#define CPKT_PDF_LANG_HR "hr"
#define CPKT_PDF_LANG_HU "hu"
#define CPKT_PDF_LANG_HY "hy"
#define CPKT_PDF_LANG_IA "ia"
#define CPKT_PDF_LANG_IE "ie"
#define CPKT_PDF_LANG_IK "ik"
#define CPKT_PDF_LANG_IN "in"
#define CPKT_PDF_LANG_IS "is"
#define CPKT_PDF_LANG_IT "it"
#define CPKT_PDF_LANG_IW "iw"
#define CPKT_PDF_LANG_JA "ja"
#define CPKT_PDF_LANG_JI "ji"
#define CPKT_PDF_LANG_JW "jw"
#define CPKT_PDF_LANG_KA "ka"
#define CPKT_PDF_LANG_KK "kk"
#define CPKT_PDF_LANG_KL "kl"
#define CPKT_PDF_LANG_KM "km"
#define CPKT_PDF_LANG_KN "kn"
#define CPKT_PDF_LANG_KO "ko"
#define CPKT_PDF_LANG_KS "ks"
#define CPKT_PDF_LANG_KU "ku"
#define CPKT_PDF_LANG_KY "ky"
#define CPKT_PDF_LANG_LA "la"
#define CPKT_PDF_LANG_LN "ln"
#define CPKT_PDF_LANG_LO "lo"
#define CPKT_PDF_LANG_LT "lt"
#define CPKT_PDF_LANG_LV "lv"
#define CPKT_PDF_LANG_MG "mg"
#define CPKT_PDF_LANG_MI "mi"
#define CPKT_PDF_LANG_MK "mk"
#define CPKT_PDF_LANG_ML "ml"
#define CPKT_PDF_LANG_MN "mn"
#define CPKT_PDF_LANG_MO "mo"
#define CPKT_PDF_LANG_MR "mr"
#define CPKT_PDF_LANG_MS "ms"
#define CPKT_PDF_LANG_MT "mt"
#define CPKT_PDF_LANG_MY "my"
#define CPKT_PDF_LANG_NA "na"
#define CPKT_PDF_LANG_NE "ne"
#define CPKT_PDF_LANG_NL "nl"
#define CPKT_PDF_LANG_NO "no"
#define CPKT_PDF_LANG_OC "oc"
#define CPKT_PDF_LANG_OM "om"
#define CPKT_PDF_LANG_OR "or"
#define CPKT_PDF_LANG_PA "pa"
#define CPKT_PDF_LANG_PL "pl"
#define CPKT_PDF_LANG_PS "ps"
#define CPKT_PDF_LANG_PT "pt"
#define CPKT_PDF_LANG_QU "qu"
#define CPKT_PDF_LANG_RM "rm"
#define CPKT_PDF_LANG_RN "rn"
#define CPKT_PDF_LANG_RO "ro"
#define CPKT_PDF_LANG_RU "ru"
#define CPKT_PDF_LANG_RW "rw"
#define CPKT_PDF_LANG_SA "sa"
#define CPKT_PDF_LANG_SD "sd"
#define CPKT_PDF_LANG_SG "sg"
#define CPKT_PDF_LANG_SH "sh"
#define CPKT_PDF_LANG_SI "si"
#define CPKT_PDF_LANG_SK "sk"
#define CPKT_PDF_LANG_SL "sl"
#define CPKT_PDF_LANG_SM "sm"
#define CPKT_PDF_LANG_SN "sn"
#define CPKT_PDF_LANG_SO "so"
#define CPKT_PDF_LANG_SQ "sq"
#define CPKT_PDF_LANG_SR "sr"
#define CPKT_PDF_LANG_SS "ss"
#define CPKT_PDF_LANG_ST "st"
#define CPKT_PDF_LANG_SU "su"
#define CPKT_PDF_LANG_SV "sv"
#define CPKT_PDF_LANG_SW "sw"
#define CPKT_PDF_LANG_TA "ta"
#define CPKT_PDF_LANG_TE "te"
#define CPKT_PDF_LANG_TG "tg"
#define CPKT_PDF_LANG_TH "th"
#define CPKT_PDF_LANG_TI "ti"
#define CPKT_PDF_LANG_TK "tk"
#define CPKT_PDF_LANG_TL "tl"
#define CPKT_PDF_LANG_TN "tn"
#define CPKT_PDF_LANG_TR "tr"
#define CPKT_PDF_LANG_TS "ts"
#define CPKT_PDF_LANG_TT "tt"
#define CPKT_PDF_LANG_TW "tw"
#define CPKT_PDF_LANG_UK "uk"
#define CPKT_PDF_LANG_UR "ur"
#define CPKT_PDF_LANG_UZ "uz"
#define CPKT_PDF_LANG_VI "vi"
#define CPKT_PDF_LANG_VO "vo"
#define CPKT_PDF_LANG_WO "wo"
#define CPKT_PDF_LANG_XH "xh"
#define CPKT_PDF_LANG_YO "yo"
#define CPKT_PDF_LANG_ZH "zh"
#define CPKT_PDF_LANG_ZU "zu"

#define CPKT_PDF_GMODE_PAGE_DESCRIPTION 0x0001
#define CPKT_PDF_GMODE_PATH_OBJECT 0x0002
#define CPKT_PDF_GMODE_TEXT_OBJECT 0x0004
#define CPKT_PDF_GMODE_CLIPPING_PATH 0x0008
#define CPKT_PDF_GMODE_SHADING 0x0010
#define CPKT_PDF_GMODE_INLINE_IMAGE 0x0020
#define CPKT_PDF_GMODE_EXTERNAL_OBJECT 0x0040

/** Opaque libHaru object handle; the containing document owns child objects. */
typedef void *CPKT_PDF_HANDLE;
typedef CPKT_PDF_HANDLE CPKT_PDF_Boolean;
typedef CPKT_PDF_HANDLE CPKT_PDF_Doc;
typedef CPKT_PDF_HANDLE CPKT_PDF_Page;
typedef CPKT_PDF_HANDLE CPKT_PDF_Pages;
typedef CPKT_PDF_HANDLE CPKT_PDF_Stream;
typedef CPKT_PDF_HANDLE CPKT_PDF_Image;
typedef CPKT_PDF_HANDLE CPKT_PDF_Font;
typedef CPKT_PDF_HANDLE CPKT_PDF_Outline;
typedef CPKT_PDF_HANDLE CPKT_PDF_Encoder;
typedef CPKT_PDF_HANDLE CPKT_PDF_3DMeasure;
typedef CPKT_PDF_HANDLE CPKT_PDF_ExData;
typedef CPKT_PDF_HANDLE CPKT_PDF_Destination;
typedef CPKT_PDF_HANDLE CPKT_PDF_XObject;
typedef CPKT_PDF_HANDLE CPKT_PDF_Annotation;
typedef CPKT_PDF_HANDLE CPKT_PDF_ExtGState;
typedef CPKT_PDF_HANDLE CPKT_PDF_FontDef;
typedef CPKT_PDF_HANDLE CPKT_PDF_U3D;
typedef CPKT_PDF_HANDLE CPKT_PDF_JavaScript;
typedef CPKT_PDF_HANDLE CPKT_PDF_Error;
typedef CPKT_PDF_HANDLE CPKT_PDF_MMgr;
typedef CPKT_PDF_HANDLE CPKT_PDF_Dict;
typedef CPKT_PDF_HANDLE CPKT_PDF_EmbeddedFile;
typedef CPKT_PDF_HANDLE CPKT_PDF_OutputIntent;
typedef CPKT_PDF_HANDLE CPKT_PDF_Xref;
typedef CPKT_PDF_HANDLE CPKT_PDF_Shading;

/** Returns a static libHaru version string; do not free it. */
CPKT_PDF_API const char *cpkt_pdf_get_version(void);
/** Creates an owned document with caller allocator and error callbacks; release
 * it with cpkt_pdf_free(). */
CPKT_PDF_API CPKT_PDF_Doc cpkt_pdf_new_ex(CPKT_PDF_Error_Handler user_error_fn,
                                          CPKT_PDF_Alloc_Func user_alloc_fn,
                                          CPKT_PDF_Free_Func user_free_fn,
                                          CPKT_PDF_UINT mem_pool_buf_size,
                                          void *user_data);
/** Creates an owned document; release it with cpkt_pdf_free(). */
CPKT_PDF_API CPKT_PDF_Doc cpkt_pdf_new(CPKT_PDF_Error_Handler user_error_fn,
                                       void *user_data);
/** Calls libHaru's HPDF_SetErrorHandler with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_error_handler(
    CPKT_PDF_Doc pdf, CPKT_PDF_Error_Handler user_error_fn);
/** Releases the document and all objects owned by it. */
CPKT_PDF_API void cpkt_pdf_free(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_GetDocMMgr with C89 facade types. */
CPKT_PDF_API CPKT_PDF_MMgr cpkt_pdf_get_doc_m_mgr(CPKT_PDF_Doc doc);
/** Starts a new document in this handle; previously created document objects
 * become invalid. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_new_doc(CPKT_PDF_Doc pdf);
/** Releases the current document and its page, font, and image objects. */
CPKT_PDF_API void cpkt_pdf_free_doc(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_HasDoc with C89 facade types. */
CPKT_PDF_API CPKT_PDF_BOOL cpkt_pdf_has_doc(CPKT_PDF_Doc pdf);
/** Releases every document owned by this handle. */
CPKT_PDF_API void cpkt_pdf_free_doc_all(CPKT_PDF_Doc pdf);
/** Serializes the full PDF into libHaru's in-memory stream; this is buffered
 * output. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_save_to_stream(CPKT_PDF_Doc pdf);
/** Serializes a full PDF into temporary memory, then copies up to the input
 * size into buf and writes the copied byte count to size. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_get_contents(CPKT_PDF_Doc pdf,
                                                   CPKT_PDF_BYTE *buf,
                                                   CPKT_PDF_UINT32 *size);
/** Returns the size of the document's saved in-memory stream. */
CPKT_PDF_API CPKT_PDF_UINT32 cpkt_pdf_get_stream_size(CPKT_PDF_Doc pdf);
/** Reads up to the input size from the saved in-memory PDF stream and writes
 * the byte count read to size. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_read_from_stream(CPKT_PDF_Doc pdf,
                                                       CPKT_PDF_BYTE *buf,
                                                       CPKT_PDF_UINT32 *size);
/** Rewinds the saved in-memory stream for another read. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_reset_stream(CPKT_PDF_Doc pdf);
/** Serializes the PDF to the named file; the caller owns the pathname. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_save_to_file(CPKT_PDF_Doc pdf,
                                                   const char *file_name);
/** Calls libHaru's HPDF_GetError with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_get_error(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_GetErrorDetail with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_get_error_detail(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_ResetError with C89 facade types. */
CPKT_PDF_API void cpkt_pdf_reset_error(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_CheckError with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_check_error(CPKT_PDF_Error error);
/** Calls libHaru's HPDF_SetPagesConfiguration with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_pages_configuration(
    CPKT_PDF_Doc pdf, CPKT_PDF_UINT page_per_pages);
/** Calls libHaru's HPDF_GetPageByIndex with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Page cpkt_pdf_get_page_by_index(CPKT_PDF_Doc pdf,
                                                      CPKT_PDF_UINT index);
/** Calls libHaru's HPDF_SetPDFAConformance with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_set_pdfa_conformance(CPKT_PDF_Doc pdf, CPKT_PDF_PDFAType pdfa_type);
/** Calls libHaru's HPDF_AddPDFAXmpExtension with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_add_pdfa_xmp_extension(CPKT_PDF_Doc pdf, const char *xmp_description);
/** Calls libHaru's HPDF_AppendOutputIntents with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_append_output_intents(
    CPKT_PDF_Doc pdf, const char *iccname, CPKT_PDF_Dict iccdict);
/** Calls libHaru's HPDF_GetPageMMgr with C89 facade types. */
CPKT_PDF_API CPKT_PDF_MMgr cpkt_pdf_get_page_m_mgr(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_GetPageLayout with C89 facade types. */
CPKT_PDF_API CPKT_PDF_PageLayout cpkt_pdf_get_page_layout(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_SetPageLayout with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_set_page_layout(CPKT_PDF_Doc pdf, CPKT_PDF_PageLayout layout);
/** Calls libHaru's HPDF_GetPageMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_PageMode cpkt_pdf_get_page_mode(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_SetPageMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_page_mode(CPKT_PDF_Doc pdf,
                                                    CPKT_PDF_PageMode mode);
/** Calls libHaru's HPDF_GetViewerPreference with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_get_viewer_preference(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_SetViewerPreference with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_set_viewer_preference(CPKT_PDF_Doc pdf, CPKT_PDF_UINT value);
/** Calls libHaru's HPDF_SetOpenAction with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_set_open_action(CPKT_PDF_Doc pdf, CPKT_PDF_Destination open_action);
/** Calls libHaru's HPDF_GetCurrentPage with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Page cpkt_pdf_get_current_page(CPKT_PDF_Doc pdf);
/** Adds a page owned by the current document; free the document to release it.
 */
CPKT_PDF_API CPKT_PDF_Page cpkt_pdf_add_page(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_InsertPage with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Page cpkt_pdf_insert_page(CPKT_PDF_Doc pdf,
                                                CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_SetWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_width(CPKT_PDF_Page page,
                                                     CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetHeight with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_height(CPKT_PDF_Page page,
                                                      CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetBoundary with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_boundary(
    CPKT_PDF_Page page, CPKT_PDF_PageBoundary boundary, CPKT_PDF_REAL left,
    CPKT_PDF_REAL bottom, CPKT_PDF_REAL right, CPKT_PDF_REAL top);
/** Calls libHaru's HPDF_Page_SetSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_size(CPKT_PDF_Page page, CPKT_PDF_PageSizes size,
                       CPKT_PDF_PageDirection direction);
/** Calls libHaru's HPDF_Page_SetRotate with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_rotate(CPKT_PDF_Page page,
                                                      CPKT_PDF_UINT16 angle);
/** Calls libHaru's HPDF_Page_SetZoom with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_zoom(CPKT_PDF_Page page,
                                                    CPKT_PDF_REAL zoom);
/** Calls libHaru's HPDF_GetFont with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Font cpkt_pdf_get_font(CPKT_PDF_Doc pdf,
                                             const char *font_name,
                                             const char *encoding_name);
/** Calls libHaru's HPDF_LoadType1FontFromFile with C89 facade types. */
CPKT_PDF_API const char *
cpkt_pdf_load_type1_font_from_file(CPKT_PDF_Doc pdf, const char *afm_file_name,
                                   const char *data_file_name);
/** Calls libHaru's HPDF_GetTTFontDefFromFile with C89 facade types. */
CPKT_PDF_API CPKT_PDF_FontDef cpkt_pdf_get_tt_font_def_from_file(
    CPKT_PDF_Doc pdf, const char *file_name, CPKT_PDF_BOOL embedding);
/** Calls libHaru's HPDF_LoadTTFontFromFile with C89 facade types. */
CPKT_PDF_API const char *
cpkt_pdf_load_tt_font_from_file(CPKT_PDF_Doc pdf, const char *file_name,
                                CPKT_PDF_BOOL embedding);
/** Calls libHaru's HPDF_LoadTTFontFromFile2 with C89 facade types. */
CPKT_PDF_API const char *
cpkt_pdf_load_tt_font_from_file2(CPKT_PDF_Doc pdf, const char *file_name,
                                 CPKT_PDF_UINT index, CPKT_PDF_BOOL embedding);
/** Calls libHaru's HPDF_LoadTTFontFromMemory with C89 facade types. */
CPKT_PDF_API const char *
cpkt_pdf_load_tt_font_from_memory(CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buffer,
                                  CPKT_PDF_UINT size, CPKT_PDF_BOOL embedding);
/** Calls libHaru's HPDF_AddPageLabel with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_add_page_label(
    CPKT_PDF_Doc pdf, CPKT_PDF_UINT page_num, CPKT_PDF_PageNumStyle style,
    CPKT_PDF_UINT first_page, const char *prefix);
/** Calls libHaru's HPDF_UseJPFonts with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_jp_fonts(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseKRFonts with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_kr_fonts(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseCNSFonts with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_cns_fonts(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseCNTFonts with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_cnt_fonts(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_CreateOutline with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Outline cpkt_pdf_create_outline(CPKT_PDF_Doc pdf,
                                                      CPKT_PDF_Outline parent,
                                                      const char *title,
                                                      CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Outline_SetOpened with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_outline_set_opened(CPKT_PDF_Outline outline, CPKT_PDF_BOOL opened);
/** Calls libHaru's HPDF_Outline_SetDestination with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_outline_set_destination(
    CPKT_PDF_Outline outline, CPKT_PDF_Destination dst);
/** Calls libHaru's HPDF_Page_CreateDestination with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Destination
cpkt_pdf_page_create_destination(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Destination_SetXYZ with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_xyz(CPKT_PDF_Destination dst, CPKT_PDF_REAL left,
                             CPKT_PDF_REAL top, CPKT_PDF_REAL zoom);
/** Calls libHaru's HPDF_Destination_SetFit with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_fit(CPKT_PDF_Destination dst);
/** Calls libHaru's HPDF_Destination_SetFitH with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_fit_h(CPKT_PDF_Destination dst, CPKT_PDF_REAL top);
/** Calls libHaru's HPDF_Destination_SetFitV with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_fit_v(CPKT_PDF_Destination dst, CPKT_PDF_REAL left);
/** Calls libHaru's HPDF_Destination_SetFitR with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_destination_set_fit_r(
    CPKT_PDF_Destination dst, CPKT_PDF_REAL left, CPKT_PDF_REAL bottom,
    CPKT_PDF_REAL right, CPKT_PDF_REAL top);
/** Calls libHaru's HPDF_Destination_SetFitB with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_fit_b(CPKT_PDF_Destination dst);
/** Calls libHaru's HPDF_Destination_SetFitBH with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_fit_bh(CPKT_PDF_Destination dst, CPKT_PDF_REAL top);
/** Calls libHaru's HPDF_Destination_SetFitBV with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_destination_set_fit_bv(CPKT_PDF_Destination dst, CPKT_PDF_REAL left);
/** Calls libHaru's HPDF_GetEncoder with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Encoder cpkt_pdf_get_encoder(CPKT_PDF_Doc pdf,
                                                   const char *encoding_name);
/** Calls libHaru's HPDF_GetCurrentEncoder with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Encoder cpkt_pdf_get_current_encoder(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_SetCurrentEncoder with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_set_current_encoder(CPKT_PDF_Doc pdf, const char *encoding_name);
/** Calls libHaru's HPDF_Encoder_GetType with C89 facade types. */
CPKT_PDF_API CPKT_PDF_EncoderType
cpkt_pdf_encoder_get_type(CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Encoder_GetByteType with C89 facade types. */
CPKT_PDF_API CPKT_PDF_ByteType cpkt_pdf_encoder_get_byte_type(
    CPKT_PDF_Encoder encoder, const char *text, CPKT_PDF_UINT index);
/** Calls libHaru's HPDF_Encoder_GetUnicode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UNICODE
cpkt_pdf_encoder_get_unicode(CPKT_PDF_Encoder encoder, CPKT_PDF_UINT16 code);
/** Calls libHaru's HPDF_Encoder_GetWritingMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_WritingMode
cpkt_pdf_encoder_get_writing_mode(CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_UseJPEncodings with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_jp_encodings(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseKREncodings with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_kr_encodings(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseCNSEncodings with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_cns_encodings(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseCNTEncodings with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_cnt_encodings(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_UseUTFEncodings with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_use_utf_encodings(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_Page_CreateXObjectFromImage with C89 facade types. */
CPKT_PDF_API CPKT_PDF_XObject cpkt_pdf_page_create_x_object_from_image(
    CPKT_PDF_Doc pdf, CPKT_PDF_Page page, CPKT_PDF_Rect rect,
    CPKT_PDF_Image image, CPKT_PDF_BOOL zoom);
/** Calls libHaru's HPDF_Page_CreateXObjectAsWhiteRect with C89 facade types. */
CPKT_PDF_API CPKT_PDF_XObject cpkt_pdf_page_create_x_object_as_white_rect(
    CPKT_PDF_Doc pdf, CPKT_PDF_Page page, CPKT_PDF_Rect rect);
/** Calls libHaru's HPDF_Page_Create3DAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create3_d_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, CPKT_PDF_BOOL tb, CPKT_PDF_BOOL np,
    CPKT_PDF_U3D u3d, CPKT_PDF_Image ap);
/** Calls libHaru's HPDF_Page_CreateTextAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation
cpkt_pdf_page_create_text_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateFreeTextAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_free_text_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, const char *text,
    CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateLineAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_line_annot(
    CPKT_PDF_Page page, const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateWidgetAnnot_WhiteOnlyWhilePrint with C89
 * facade types. */
CPKT_PDF_API CPKT_PDF_Annotation
cpkt_pdf_page_create_widget_annot_white_only_while_print(CPKT_PDF_Doc pdf,
                                                         CPKT_PDF_Page page,
                                                         CPKT_PDF_Rect rect);
/** Calls libHaru's HPDF_Page_CreateWidgetAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation
cpkt_pdf_page_create_widget_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect);
/** Calls libHaru's HPDF_Page_CreateLinkAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_link_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, CPKT_PDF_Destination dst);
/** Calls libHaru's HPDF_Page_CreateURILinkAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_uri_link_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, const char *uri);
/** Calls libHaru's HPDF_Page_CreateHighlightAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_highlight_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, const char *text,
    CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateUnderlineAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_underline_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, const char *text,
    CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateSquigglyAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation
cpkt_pdf_page_create_squiggly_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                    const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateStrikeOutAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_strike_out_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, const char *text,
    CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreatePopupAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_popup_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, CPKT_PDF_Annotation parent);
/** Calls libHaru's HPDF_Page_CreateStampAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_stamp_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, CPKT_PDF_StampAnnotName name,
    const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateProjectionAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation cpkt_pdf_page_create_projection_annot(
    CPKT_PDF_Page page, CPKT_PDF_Rect rect, const char *text,
    CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateSquareAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation
cpkt_pdf_page_create_square_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                  const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_Page_CreateCircleAnnot with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Annotation
cpkt_pdf_page_create_circle_annot(CPKT_PDF_Page page, CPKT_PDF_Rect rect,
                                  const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_LinkAnnot_SetHighlightMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_link_annot_set_highlight_mode(
    CPKT_PDF_Annotation annot, CPKT_PDF_AnnotHighlightMode mode);
/** Calls libHaru's HPDF_LinkAnnot_SetJavaScript with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_link_annot_set_java_script(
    CPKT_PDF_Annotation annot, CPKT_PDF_JavaScript javascript);
/** Calls libHaru's HPDF_LinkAnnot_SetBorderStyle with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_link_annot_set_border_style(
    CPKT_PDF_Annotation annot, CPKT_PDF_REAL width, CPKT_PDF_UINT16 dash_on,
    CPKT_PDF_UINT16 dash_off);
/** Calls libHaru's HPDF_TextAnnot_SetIcon with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_text_annot_set_icon(
    CPKT_PDF_Annotation annot, CPKT_PDF_AnnotIcon icon);
/** Calls libHaru's HPDF_TextAnnot_SetOpened with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_text_annot_set_opened(CPKT_PDF_Annotation annot, CPKT_PDF_BOOL opened);
/** Calls libHaru's HPDF_Annot_SetRGBColor with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_annot_set_rgb_color(
    CPKT_PDF_Annotation annot, CPKT_PDF_RGBColor color);
/** Calls libHaru's HPDF_Annot_SetCMYKColor with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_annot_set_cmyk_color(
    CPKT_PDF_Annotation annot, CPKT_PDF_CMYKColor color);
/** Calls libHaru's HPDF_Annot_SetGrayColor with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_annot_set_gray_color(CPKT_PDF_Annotation annot, CPKT_PDF_REAL color);
/** Calls libHaru's HPDF_Annot_SetNoColor with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_annot_set_no_color(CPKT_PDF_Annotation annot);
/** Calls libHaru's HPDF_MarkupAnnot_SetTitle with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_title(CPKT_PDF_Annotation annot, const char *name);
/** Calls libHaru's HPDF_MarkupAnnot_SetSubject with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_subject(CPKT_PDF_Annotation annot, const char *name);
/** Calls libHaru's HPDF_MarkupAnnot_SetCreationDate with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_creation_date(
    CPKT_PDF_Annotation annot, CPKT_PDF_Date value);
/** Calls libHaru's HPDF_MarkupAnnot_SetTransparency with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_transparency(
    CPKT_PDF_Annotation annot, CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_MarkupAnnot_SetIntent with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_intent(
    CPKT_PDF_Annotation annot, CPKT_PDF_AnnotIntent intent);
/** Calls libHaru's HPDF_MarkupAnnot_SetPopup with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_popup(
    CPKT_PDF_Annotation annot, CPKT_PDF_Annotation popup);
/** Calls libHaru's HPDF_MarkupAnnot_SetRectDiff with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_rect_diff(
    CPKT_PDF_Annotation annot, CPKT_PDF_Rect rect);
/** Calls libHaru's HPDF_MarkupAnnot_SetCloudEffect with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_cloud_effect(
    CPKT_PDF_Annotation annot, CPKT_PDF_INT cloudIntensity);
/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorRGBColor with C89 facade types.
 */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_interior_rgb_color(
    CPKT_PDF_Annotation annot, CPKT_PDF_RGBColor color);
/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorCMYKColor with C89 facade types.
 */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_interior_cmyk_color(
    CPKT_PDF_Annotation annot, CPKT_PDF_CMYKColor color);
/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorGrayColor with C89 facade types.
 */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_markup_annot_set_interior_gray_color(
    CPKT_PDF_Annotation annot, CPKT_PDF_REAL color);
/** Calls libHaru's HPDF_MarkupAnnot_SetInteriorTransparent with C89 facade
 * types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_markup_annot_set_interior_transparent(CPKT_PDF_Annotation annot);
/** Calls libHaru's HPDF_TextMarkupAnnot_SetQuadPoints with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_text_markup_annot_set_quad_points(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point lb, CPKT_PDF_Point rb,
    CPKT_PDF_Point rt, CPKT_PDF_Point lt);
/** Calls libHaru's HPDF_Annot_Set3DView with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_annot_set3_d_view(CPKT_PDF_MMgr mmgr, CPKT_PDF_Annotation annot,
                           CPKT_PDF_Annotation annot3d, CPKT_PDF_Dict view);
/** Calls libHaru's HPDF_PopupAnnot_SetOpened with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_popup_annot_set_opened(
    CPKT_PDF_Annotation annot, CPKT_PDF_BOOL opened);
/** Calls libHaru's HPDF_FreeTextAnnot_SetLineEndingStyle with C89 facade types.
 */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_free_text_annot_set_line_ending_style(
    CPKT_PDF_Annotation annot, CPKT_PDF_LineAnnotEndingStyle startStyle,
    CPKT_PDF_LineAnnotEndingStyle endStyle);
/** Calls libHaru's HPDF_FreeTextAnnot_Set3PointCalloutLine with C89 facade
 * types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_free_text_annot_set3_point_callout_line(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point startPoint,
    CPKT_PDF_Point kneePoint, CPKT_PDF_Point endPoint);
/** Calls libHaru's HPDF_FreeTextAnnot_Set2PointCalloutLine with C89 facade
 * types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_free_text_annot_set2_point_callout_line(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point startPoint,
    CPKT_PDF_Point endPoint);
/** Calls libHaru's HPDF_FreeTextAnnot_SetDefaultStyle with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_free_text_annot_set_default_style(
    CPKT_PDF_Annotation annot, const char *style);
/** Calls libHaru's HPDF_LineAnnot_SetPosition with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_line_annot_set_position(
    CPKT_PDF_Annotation annot, CPKT_PDF_Point startPoint,
    CPKT_PDF_LineAnnotEndingStyle startStyle, CPKT_PDF_Point endPoint,
    CPKT_PDF_LineAnnotEndingStyle endStyle);
/** Calls libHaru's HPDF_LineAnnot_SetLeader with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_line_annot_set_leader(
    CPKT_PDF_Annotation annot, CPKT_PDF_INT leaderLen,
    CPKT_PDF_INT leaderExtLen, CPKT_PDF_INT leaderOffsetLen);
/** Calls libHaru's HPDF_LineAnnot_SetCaption with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_line_annot_set_caption(
    CPKT_PDF_Annotation annot, CPKT_PDF_BOOL showCaption,
    CPKT_PDF_LineAnnotCapPosition position, CPKT_PDF_INT horzOffset,
    CPKT_PDF_INT vertOffset);
/** Calls libHaru's HPDF_Annotation_SetBorderStyle with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_annotation_set_border_style(
    CPKT_PDF_Annotation annot, CPKT_PDF_BSSubtype subtype, CPKT_PDF_REAL width,
    CPKT_PDF_UINT16 dash_on, CPKT_PDF_UINT16 dash_off,
    CPKT_PDF_UINT16 dash_phase);
/** Calls libHaru's HPDF_ProjectionAnnot_SetExData with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_projection_annot_set_ex_data(
    CPKT_PDF_Annotation annot, CPKT_PDF_ExData exdata);
/** Calls libHaru's HPDF_Page_Create3DC3DMeasure with C89 facade types. */
CPKT_PDF_API CPKT_PDF_3DMeasure cpkt_pdf_page_create3_dc3_d_measure(
    CPKT_PDF_Page page, CPKT_PDF_Point3D firstanchorpoint,
    CPKT_PDF_Point3D textanchorpoint);
/** Calls libHaru's HPDF_Page_CreatePD33DMeasure with C89 facade types. */
CPKT_PDF_API CPKT_PDF_3DMeasure cpkt_pdf_page_create_pd33_d_measure(
    CPKT_PDF_Page page, CPKT_PDF_Point3D annotationPlaneNormal,
    CPKT_PDF_Point3D firstAnchorPoint, CPKT_PDF_Point3D secondAnchorPoint,
    CPKT_PDF_Point3D leaderLinesDirection,
    CPKT_PDF_Point3D measurementValuePoint, CPKT_PDF_Point3D textYDirection,
    CPKT_PDF_REAL value, const char *unitsString);
/** Calls libHaru's HPDF_3DMeasure_SetName with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_3_d_measure_set_name(CPKT_PDF_3DMeasure measure, const char *name);
/** Calls libHaru's HPDF_3DMeasure_SetColor with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_d_measure_set_color(
    CPKT_PDF_3DMeasure measure, CPKT_PDF_RGBColor color);
/** Calls libHaru's HPDF_3DMeasure_SetTextSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_d_measure_set_text_size(
    CPKT_PDF_3DMeasure measure, CPKT_PDF_REAL textsize);
/** Calls libHaru's HPDF_3DC3DMeasure_SetTextBoxSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_dc3_d_measure_set_text_box_size(
    CPKT_PDF_3DMeasure measure, CPKT_PDF_INT32 x, CPKT_PDF_INT32 y);
/** Calls libHaru's HPDF_3DC3DMeasure_SetText with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_dc3_d_measure_set_text(
    CPKT_PDF_3DMeasure measure, const char *text, CPKT_PDF_Encoder encoder);
/** Calls libHaru's HPDF_3DC3DMeasure_SetProjectionAnotation with C89 facade
 * types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_dc3_d_measure_set_projection_anotation(
    CPKT_PDF_3DMeasure measure, CPKT_PDF_Annotation projectionanotation);
/** Calls libHaru's HPDF_Page_Create3DAnnotExData with C89 facade types. */
CPKT_PDF_API CPKT_PDF_ExData
cpkt_pdf_page_create3_d_annot_ex_data(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_3DAnnotExData_Set3DMeasurement with C89 facade types.
 */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_d_annot_ex_data_set3_d_measurement(
    CPKT_PDF_ExData exdata, CPKT_PDF_3DMeasure measure);
/** Calls libHaru's HPDF_Page_Create3DView with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Dict
cpkt_pdf_page_create3_d_view(CPKT_PDF_Page page, CPKT_PDF_U3D u3d,
                             CPKT_PDF_Annotation annot3d, const char *name);
/** Calls libHaru's HPDF_3DView_Add3DC3DMeasure with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_3_d_view_add3_dc3_d_measure(
    CPKT_PDF_Dict view, CPKT_PDF_3DMeasure measure);
/** Loads a PNG byte buffer as a document-owned image. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_load_png_image_from_mem(
    CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buffer, CPKT_PDF_UINT size);
/** Loads a PNG file as a document-owned image. */
CPKT_PDF_API CPKT_PDF_Image
cpkt_pdf_load_png_image_from_file(CPKT_PDF_Doc pdf, const char *filename);
/** Calls libHaru's HPDF_LoadPngImageFromFile2 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image
cpkt_pdf_load_png_image_from_file2(CPKT_PDF_Doc pdf, const char *filename);
/** Calls libHaru's HPDF_LoadJpegImageFromFile with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image
cpkt_pdf_load_jpeg_image_from_file(CPKT_PDF_Doc pdf, const char *filename);
/** Calls libHaru's HPDF_LoadJpegImageFromMem with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_load_jpeg_image_from_mem(
    CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buffer, CPKT_PDF_UINT size);
/** Calls libHaru's HPDF_LoadU3DFromFile with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_load_u3_d_from_file(CPKT_PDF_Doc pdf,
                                                         const char *filename);
/** Calls libHaru's HPDF_LoadU3DFromMem with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_load_u3_d_from_mem(
    CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buffer, CPKT_PDF_UINT size);
/** Calls libHaru's HPDF_Image_LoadRaw1BitImageFromMem with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_image_load_raw1_bit_image_from_mem(
    CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buf, CPKT_PDF_UINT width,
    CPKT_PDF_UINT height, CPKT_PDF_UINT line_width, CPKT_PDF_BOOL black_is1,
    CPKT_PDF_BOOL top_is_first);
/** Calls libHaru's HPDF_LoadRawImageFromFile with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_load_raw_image_from_file(
    CPKT_PDF_Doc pdf, const char *filename, CPKT_PDF_UINT width,
    CPKT_PDF_UINT height, CPKT_PDF_ColorSpace color_space);
/** Calls libHaru's HPDF_LoadRawImageFromMem with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Image cpkt_pdf_load_raw_image_from_mem(
    CPKT_PDF_Doc pdf, const CPKT_PDF_BYTE *buf, CPKT_PDF_UINT width,
    CPKT_PDF_UINT height, CPKT_PDF_ColorSpace color_space,
    CPKT_PDF_UINT bits_per_component);
/** Calls libHaru's HPDF_Image_AddSMask with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_image_add_s_mask(CPKT_PDF_Image image,
                                                       CPKT_PDF_Image smask);
/** Calls libHaru's HPDF_Image_GetSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Point cpkt_pdf_image_get_size(CPKT_PDF_Image image);
/** Calls libHaru's HPDF_Image_GetSize2 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_image_get_size2(CPKT_PDF_Image image,
                                                      CPKT_PDF_Point *size);
/** Calls libHaru's HPDF_Image_GetWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_image_get_width(CPKT_PDF_Image image);
/** Calls libHaru's HPDF_Image_GetHeight with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_image_get_height(CPKT_PDF_Image image);
/** Calls libHaru's HPDF_Image_GetBitsPerComponent with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT
cpkt_pdf_image_get_bits_per_component(CPKT_PDF_Image image);
/** Calls libHaru's HPDF_Image_GetColorSpace with C89 facade types. */
CPKT_PDF_API const char *cpkt_pdf_image_get_color_space(CPKT_PDF_Image image);
/** Calls libHaru's HPDF_Image_SetColorMask with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_image_set_color_mask(
    CPKT_PDF_Image image, CPKT_PDF_UINT rmin, CPKT_PDF_UINT rmax,
    CPKT_PDF_UINT gmin, CPKT_PDF_UINT gmax, CPKT_PDF_UINT bmin,
    CPKT_PDF_UINT bmax);
/** Calls libHaru's HPDF_Image_SetMaskImage with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_image_set_mask_image(CPKT_PDF_Image image, CPKT_PDF_Image mask_image);
/** Calls libHaru's HPDF_SetInfoAttr with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_info_attr(CPKT_PDF_Doc pdf,
                                                    CPKT_PDF_InfoType type,
                                                    const char *value);
/** Calls libHaru's HPDF_GetInfoAttr with C89 facade types. */
CPKT_PDF_API const char *cpkt_pdf_get_info_attr(CPKT_PDF_Doc pdf,
                                                CPKT_PDF_InfoType type);
/** Calls libHaru's HPDF_SetInfoDateAttr with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_info_date_attr(CPKT_PDF_Doc pdf,
                                                         CPKT_PDF_InfoType type,
                                                         CPKT_PDF_Date value);
/** Calls libHaru's HPDF_SetPassword with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_password(CPKT_PDF_Doc pdf,
                                                   const char *owner_passwd,
                                                   const char *user_passwd);
/** Calls libHaru's HPDF_SetPermission with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_permission(CPKT_PDF_Doc pdf,
                                                     CPKT_PDF_UINT permission);
/** Calls libHaru's HPDF_SetEncryptionMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_encryption_mode(
    CPKT_PDF_Doc pdf, CPKT_PDF_EncryptMode mode, CPKT_PDF_UINT key_len);
/** Calls libHaru's HPDF_SetCompressionMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_set_compression_mode(CPKT_PDF_Doc pdf,
                                                           CPKT_PDF_UINT mode);
/** Calls libHaru's HPDF_Font_GetFontName with C89 facade types. */
CPKT_PDF_API const char *cpkt_pdf_font_get_font_name(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_GetEncodingName with C89 facade types. */
CPKT_PDF_API const char *cpkt_pdf_font_get_encoding_name(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_GetUnicodeWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_INT
cpkt_pdf_font_get_unicode_width(CPKT_PDF_Font font, CPKT_PDF_UNICODE code);
/** Calls libHaru's HPDF_Font_GetBBox with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Box cpkt_pdf_font_get_b_box(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_GetAscent with C89 facade types. */
CPKT_PDF_API CPKT_PDF_INT cpkt_pdf_font_get_ascent(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_GetDescent with C89 facade types. */
CPKT_PDF_API CPKT_PDF_INT cpkt_pdf_font_get_descent(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_GetXHeight with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_font_get_x_height(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_GetCapHeight with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_font_get_cap_height(CPKT_PDF_Font font);
/** Calls libHaru's HPDF_Font_TextWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_TextWidth cpkt_pdf_font_text_width(
    CPKT_PDF_Font font, const CPKT_PDF_BYTE *text, CPKT_PDF_UINT len);
/** Calls libHaru's HPDF_Font_MeasureText with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_font_measure_text(
    CPKT_PDF_Font font, const CPKT_PDF_BYTE *text, CPKT_PDF_UINT len,
    CPKT_PDF_REAL width, CPKT_PDF_REAL font_size, CPKT_PDF_REAL char_space,
    CPKT_PDF_REAL word_space, CPKT_PDF_BOOL wordwrap,
    CPKT_PDF_REAL *real_width);
/** Calls libHaru's HPDF_AttachFile with C89 facade types. */
CPKT_PDF_API CPKT_PDF_EmbeddedFile cpkt_pdf_attach_file(CPKT_PDF_Doc pdf,
                                                        const char *file);
/** Calls libHaru's HPDF_EmbeddedFile_SetName with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_embedded_file_set_name(CPKT_PDF_EmbeddedFile emfile, const char *name);
/** Calls libHaru's HPDF_EmbeddedFile_SetDescription with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_description(
    CPKT_PDF_EmbeddedFile emfile, const char *new_description);
/** Calls libHaru's HPDF_EmbeddedFile_SetSubtype with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_subtype(
    CPKT_PDF_EmbeddedFile emfile, const char *subtype);
/** Calls libHaru's HPDF_EmbeddedFile_SetAFRelationship with C89 facade types.
 */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_af_relationship(
    CPKT_PDF_EmbeddedFile emfile, CPKT_PDF_AFRelationship relationship);
/** Calls libHaru's HPDF_EmbeddedFile_SetSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_size(
    CPKT_PDF_EmbeddedFile emfile, CPKT_PDF_UINT64 size);
/** Calls libHaru's HPDF_EmbeddedFile_SetCreationDate with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_creation_date(
    CPKT_PDF_EmbeddedFile emfile, CPKT_PDF_Date creationDate);
/** Calls libHaru's HPDF_EmbeddedFile_SetLastModificationDate with C89 facade
 * types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_embedded_file_set_last_modification_date(
    CPKT_PDF_EmbeddedFile emfile, CPKT_PDF_Date lastModificationDate);
/** Calls libHaru's HPDF_CreateExtGState with C89 facade types. */
CPKT_PDF_API CPKT_PDF_ExtGState cpkt_pdf_create_ext_g_state(CPKT_PDF_Doc pdf);
/** Calls libHaru's HPDF_ExtGState_SetAlphaStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_ext_g_state_set_alpha_stroke(
    CPKT_PDF_ExtGState ext_gstate, CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_ExtGState_SetAlphaFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_ext_g_state_set_alpha_fill(
    CPKT_PDF_ExtGState ext_gstate, CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_ExtGState_SetBlendMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_ext_g_state_set_blend_mode(
    CPKT_PDF_ExtGState ext_gstate, CPKT_PDF_BlendMode mode);
/** Calls libHaru's HPDF_Page_TextWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_text_width(CPKT_PDF_Page page,
                                                    const char *text);
/** Calls libHaru's HPDF_Page_MeasureText with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_page_measure_text(
    CPKT_PDF_Page page, const char *text, CPKT_PDF_REAL width,
    CPKT_PDF_BOOL wordwrap, CPKT_PDF_REAL *real_width);
/** Calls libHaru's HPDF_Page_GetWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_width(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetHeight with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_height(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetGMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT16 cpkt_pdf_page_get_g_mode(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCurrentPos with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Point cpkt_pdf_page_get_current_pos(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCurrentPos2 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_get_current_pos2(CPKT_PDF_Page page, CPKT_PDF_Point *pos);
/** Calls libHaru's HPDF_Page_GetCurrentTextPos with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Point
cpkt_pdf_page_get_current_text_pos(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCurrentTextPos2 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_get_current_text_pos2(CPKT_PDF_Page page, CPKT_PDF_Point *pos);
/** Calls libHaru's HPDF_Page_GetCurrentFont with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Font cpkt_pdf_page_get_current_font(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCurrentFontSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL
cpkt_pdf_page_get_current_font_size(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetTransMatrix with C89 facade types. */
CPKT_PDF_API CPKT_PDF_TransMatrix
cpkt_pdf_page_get_trans_matrix(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetLineWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_line_width(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetLineCap with C89 facade types. */
CPKT_PDF_API CPKT_PDF_LineCap cpkt_pdf_page_get_line_cap(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetLineJoin with C89 facade types. */
CPKT_PDF_API CPKT_PDF_LineJoin cpkt_pdf_page_get_line_join(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetMiterLimit with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_miter_limit(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetDash with C89 facade types. */
CPKT_PDF_API CPKT_PDF_DashMode cpkt_pdf_page_get_dash(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetFlat with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_flat(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCharSpace with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_char_space(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetWordSpace with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_word_space(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetHorizontalScalling with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL
cpkt_pdf_page_get_horizontal_scalling(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetTextLeading with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_text_leading(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetTextRenderingMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_TextRenderingMode
cpkt_pdf_page_get_text_rendering_mode(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetTextRaise with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_text_raise(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetTextRise with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_text_rise(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetRGBFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_RGBColor cpkt_pdf_page_get_rgb_fill(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetRGBStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_RGBColor cpkt_pdf_page_get_rgb_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCMYKFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_CMYKColor cpkt_pdf_page_get_cmyk_fill(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetCMYKStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_CMYKColor
cpkt_pdf_page_get_cmyk_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetGrayFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_gray_fill(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetGrayStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_REAL cpkt_pdf_page_get_gray_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetStrokingColorSpace with C89 facade types. */
CPKT_PDF_API CPKT_PDF_ColorSpace
cpkt_pdf_page_get_stroking_color_space(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetFillingColorSpace with C89 facade types. */
CPKT_PDF_API CPKT_PDF_ColorSpace
cpkt_pdf_page_get_filling_color_space(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetTextMatrix with C89 facade types. */
CPKT_PDF_API CPKT_PDF_TransMatrix
cpkt_pdf_page_get_text_matrix(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GetGStateDepth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_UINT cpkt_pdf_page_get_g_state_depth(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_SetLineWidth with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_line_width(CPKT_PDF_Page page, CPKT_PDF_REAL line_width);
/** Calls libHaru's HPDF_Page_SetLineCap with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_line_cap(CPKT_PDF_Page page, CPKT_PDF_LineCap line_cap);
/** Calls libHaru's HPDF_Page_SetLineJoin with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_line_join(CPKT_PDF_Page page, CPKT_PDF_LineJoin line_join);
/** Calls libHaru's HPDF_Page_SetMiterLimit with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_miter_limit(CPKT_PDF_Page page, CPKT_PDF_REAL miter_limit);
/** Calls libHaru's HPDF_Page_SetDash with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_dash(CPKT_PDF_Page page, const CPKT_PDF_REAL *dash_ptn,
                       CPKT_PDF_UINT num_param, CPKT_PDF_REAL phase);
/** Calls libHaru's HPDF_Page_SetFlat with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_flat(CPKT_PDF_Page page,
                                                    CPKT_PDF_REAL flatness);
/** Calls libHaru's HPDF_Page_SetExtGState with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_ext_g_state(
    CPKT_PDF_Page page, CPKT_PDF_ExtGState ext_gstate);
/** Calls libHaru's HPDF_Page_SetShading with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_shading(CPKT_PDF_Page page, CPKT_PDF_Shading shading);
/** Calls libHaru's HPDF_Page_GSave with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_g_save(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_GRestore with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_g_restore(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_Concat with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_concat(
    CPKT_PDF_Page page, CPKT_PDF_REAL a, CPKT_PDF_REAL b, CPKT_PDF_REAL c,
    CPKT_PDF_REAL d, CPKT_PDF_REAL x, CPKT_PDF_REAL y);
/** Calls libHaru's HPDF_Page_MoveTo with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_move_to(CPKT_PDF_Page page,
                                                   CPKT_PDF_REAL x,
                                                   CPKT_PDF_REAL y);
/** Calls libHaru's HPDF_Page_LineTo with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_line_to(CPKT_PDF_Page page,
                                                   CPKT_PDF_REAL x,
                                                   CPKT_PDF_REAL y);
/** Calls libHaru's HPDF_Page_CurveTo with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_curve_to(
    CPKT_PDF_Page page, CPKT_PDF_REAL x1, CPKT_PDF_REAL y1, CPKT_PDF_REAL x2,
    CPKT_PDF_REAL y2, CPKT_PDF_REAL x3, CPKT_PDF_REAL y3);
/** Calls libHaru's HPDF_Page_CurveTo2 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_curve_to2(CPKT_PDF_Page page,
                                                     CPKT_PDF_REAL x2,
                                                     CPKT_PDF_REAL y2,
                                                     CPKT_PDF_REAL x3,
                                                     CPKT_PDF_REAL y3);
/** Calls libHaru's HPDF_Page_CurveTo3 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_curve_to3(CPKT_PDF_Page page,
                                                     CPKT_PDF_REAL x1,
                                                     CPKT_PDF_REAL y1,
                                                     CPKT_PDF_REAL x3,
                                                     CPKT_PDF_REAL y3);
/** Calls libHaru's HPDF_Page_ClosePath with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_close_path(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_Rectangle with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_rectangle(CPKT_PDF_Page page,
                                                     CPKT_PDF_REAL x,
                                                     CPKT_PDF_REAL y,
                                                     CPKT_PDF_REAL width,
                                                     CPKT_PDF_REAL height);
/** Calls libHaru's HPDF_Page_Stroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_ClosePathStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_close_path_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_Fill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_fill(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_Eofill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_eofill(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_FillStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_fill_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_EofillStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_eofill_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_ClosePathFillStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_close_path_fill_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_ClosePathEofillStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_close_path_eofill_stroke(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_EndPath with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_end_path(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_Clip with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_clip(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_Eoclip with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_eoclip(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_BeginText with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_begin_text(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_EndText with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_end_text(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_SetCharSpace with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_char_space(CPKT_PDF_Page page,
                                                          CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetWordSpace with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_word_space(CPKT_PDF_Page page,
                                                          CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetHorizontalScalling with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_horizontal_scalling(CPKT_PDF_Page page, CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetTextLeading with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_text_leading(CPKT_PDF_Page page, CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetFontAndSize with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_font_and_size(
    CPKT_PDF_Page page, CPKT_PDF_Font font, CPKT_PDF_REAL size);
/** Calls libHaru's HPDF_Page_SetTextRenderingMode with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_text_rendering_mode(
    CPKT_PDF_Page page, CPKT_PDF_TextRenderingMode mode);
/** Calls libHaru's HPDF_Page_SetTextRise with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_text_rise(CPKT_PDF_Page page,
                                                         CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_SetTextRaise with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_text_raise(CPKT_PDF_Page page,
                                                          CPKT_PDF_REAL value);
/** Calls libHaru's HPDF_Page_MoveTextPos with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_move_text_pos(CPKT_PDF_Page page,
                                                         CPKT_PDF_REAL x,
                                                         CPKT_PDF_REAL y);
/** Calls libHaru's HPDF_Page_MoveTextPos2 with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_move_text_pos2(CPKT_PDF_Page page,
                                                          CPKT_PDF_REAL x,
                                                          CPKT_PDF_REAL y);
/** Calls libHaru's HPDF_Page_SetTextMatrix with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_text_matrix(
    CPKT_PDF_Page page, CPKT_PDF_REAL a, CPKT_PDF_REAL b, CPKT_PDF_REAL c,
    CPKT_PDF_REAL d, CPKT_PDF_REAL x, CPKT_PDF_REAL y);
/** Calls libHaru's HPDF_Page_MoveToNextLine with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_move_to_next_line(CPKT_PDF_Page page);
/** Calls libHaru's HPDF_Page_ShowText with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_show_text(CPKT_PDF_Page page,
                                                     const char *text);
/** Calls libHaru's HPDF_Page_ShowTextNextLine with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_show_text_next_line(CPKT_PDF_Page page, const char *text);
/** Calls libHaru's HPDF_Page_ShowTextNextLineEx with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_show_text_next_line_ex(
    CPKT_PDF_Page page, CPKT_PDF_REAL word_space, CPKT_PDF_REAL char_space,
    const char *text);
/** Calls libHaru's HPDF_Page_SetGrayFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_gray_fill(CPKT_PDF_Page page,
                                                         CPKT_PDF_REAL gray);
/** Calls libHaru's HPDF_Page_SetGrayStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_gray_stroke(CPKT_PDF_Page page,
                                                           CPKT_PDF_REAL gray);
/** Calls libHaru's HPDF_Page_SetRGBFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_rgb_fill(CPKT_PDF_Page page,
                                                        CPKT_PDF_REAL r,
                                                        CPKT_PDF_REAL g,
                                                        CPKT_PDF_REAL b);
/** Calls libHaru's HPDF_Page_SetRGBStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_rgb_stroke(CPKT_PDF_Page page,
                                                          CPKT_PDF_REAL r,
                                                          CPKT_PDF_REAL g,
                                                          CPKT_PDF_REAL b);
/** Calls libHaru's HPDF_Page_SetCMYKFill with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_cmyk_fill(CPKT_PDF_Page page,
                                                         CPKT_PDF_REAL c,
                                                         CPKT_PDF_REAL m,
                                                         CPKT_PDF_REAL y,
                                                         CPKT_PDF_REAL k);
/** Calls libHaru's HPDF_Page_SetCMYKStroke with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_set_cmyk_stroke(CPKT_PDF_Page page,
                                                           CPKT_PDF_REAL c,
                                                           CPKT_PDF_REAL m,
                                                           CPKT_PDF_REAL y,
                                                           CPKT_PDF_REAL k);
/** Calls libHaru's HPDF_Shading_New with C89 facade types. */
CPKT_PDF_API CPKT_PDF_Shading cpkt_pdf_shading_new(
    CPKT_PDF_Doc pdf, CPKT_PDF_ShadingType type, CPKT_PDF_ColorSpace colorSpace,
    CPKT_PDF_REAL xMin, CPKT_PDF_REAL xMax, CPKT_PDF_REAL yMin,
    CPKT_PDF_REAL yMax);
/** Calls libHaru's HPDF_Shading_AddVertexRGB with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_shading_add_vertex_rgb(
    CPKT_PDF_Shading shading,
    CPKT_PDF_Shading_FreeFormTriangleMeshEdgeFlag edgeFlag, CPKT_PDF_REAL x,
    CPKT_PDF_REAL y, CPKT_PDF_UINT8 r, CPKT_PDF_UINT8 g, CPKT_PDF_UINT8 b);
/** Calls libHaru's HPDF_Page_ExecuteXObject with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_execute_x_object(CPKT_PDF_Page page, CPKT_PDF_XObject obj);
/** Calls libHaru's HPDF_Page_New_Content_Stream with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_new_content_stream(CPKT_PDF_Page page, CPKT_PDF_Dict *new_stream);
/** Calls libHaru's HPDF_Page_Insert_Shared_Content_Stream with C89 facade
 * types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_insert_shared_content_stream(
    CPKT_PDF_Page page, CPKT_PDF_Dict shared_stream);
/** Calls libHaru's HPDF_Page_DrawImage with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_draw_image(
    CPKT_PDF_Page page, CPKT_PDF_Image image, CPKT_PDF_REAL x, CPKT_PDF_REAL y,
    CPKT_PDF_REAL width, CPKT_PDF_REAL height);
/** Calls libHaru's HPDF_Page_Circle with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_circle(CPKT_PDF_Page page,
                                                  CPKT_PDF_REAL x,
                                                  CPKT_PDF_REAL y,
                                                  CPKT_PDF_REAL ray);
/** Calls libHaru's HPDF_Page_Ellipse with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_ellipse(CPKT_PDF_Page page,
                                                   CPKT_PDF_REAL x,
                                                   CPKT_PDF_REAL y,
                                                   CPKT_PDF_REAL xray,
                                                   CPKT_PDF_REAL yray);
/** Calls libHaru's HPDF_Page_Arc with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_arc(CPKT_PDF_Page page,
                                               CPKT_PDF_REAL x, CPKT_PDF_REAL y,
                                               CPKT_PDF_REAL ray,
                                               CPKT_PDF_REAL ang1,
                                               CPKT_PDF_REAL ang2);
/** Calls libHaru's HPDF_Page_TextOut with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_text_out(CPKT_PDF_Page page,
                                                    CPKT_PDF_REAL xpos,
                                                    CPKT_PDF_REAL ypos,
                                                    const char *text);
/** Calls libHaru's HPDF_Page_TextRect with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS cpkt_pdf_page_text_rect(
    CPKT_PDF_Page page, CPKT_PDF_REAL left, CPKT_PDF_REAL top,
    CPKT_PDF_REAL right, CPKT_PDF_REAL bottom, const char *text,
    CPKT_PDF_TextAlignment align, CPKT_PDF_UINT *len);
/** Calls libHaru's HPDF_Page_SetSlideShow with C89 facade types. */
CPKT_PDF_API CPKT_PDF_STATUS
cpkt_pdf_page_set_slide_show(CPKT_PDF_Page page, CPKT_PDF_TransitionStyle type,
                             CPKT_PDF_REAL disp_time, CPKT_PDF_REAL trans_time);
/** Calls libHaru's HPDF_ICC_LoadIccFromMem with C89 facade types. */
CPKT_PDF_API CPKT_PDF_OutputIntent cpkt_pdf_icc_load_icc_from_mem(
    CPKT_PDF_Doc pdf, CPKT_PDF_MMgr mmgr, CPKT_PDF_Stream iccdata,
    CPKT_PDF_Xref xref, int numcomponent);
/** Calls libHaru's HPDF_LoadIccProfileFromFile with C89 facade types. */
CPKT_PDF_API CPKT_PDF_OutputIntent cpkt_pdf_load_icc_profile_from_file(
    CPKT_PDF_Doc pdf, const char *icc_file_name, int numcomponent);

/** @} */
#ifdef __cplusplus
}
#endif
#endif
