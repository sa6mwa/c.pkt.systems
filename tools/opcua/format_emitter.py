"""Retain upstream formatting; adapt argument reads at the C89 ABI boundary.

The parser, padding/printing and dtoa implementations are taken from the exact
installed upstream source. Argument reads, names and immediate boundary-error exit are adapted;
valid formatting retains the native parser and printing behavior.
"""
import re
from pathlib import Path
from plugin_emitter import parameters, uncomment

ARGUMENTS = {
    'int': ('INT', 'intValue', 'int'),
    'unsigned int': ('UINT', 'uintValue', 'uint'),
    'long': ('LONG', 'longValue', 'long'),
    'unsigned long': ('ULONG', 'ulongValue', 'ulong'),
    'long long': ('INT64', 'int64Value', 'int64'),
    'unsigned long long': ('UINT64', 'uint64Value', 'uint64'),
    'double': ('DOUBLE', 'doubleValue', 'double'),
    'char *': ('CHARS', 'charsValue', 'chars'),
    'void *': ('POINTER', 'pointerValue', 'pointer'),
    'UA_String': ('STRING', 'stringValue', 'string'),
    'UA_NodeId': ('NODEID', 'nodeIdValue', 'nodeid'),
    'UA_QualifiedName': ('QUALIFIEDNAME', 'qualifiedNameValue', 'qualifiedname'),
}


def emit_format(headers, source, output):
    types = uncomment((headers / 'types.h').read_text())
    for name, expected in [('UA_String_format', [('UA_String *', 'str'), ('const char *', 'format'), ('...', '')]),
                           ('UA_String_vformat', [('UA_String *', 'str'), ('const char *', 'format'), ('va_list', 'args')])]:
        found = re.search(r'\b' + name + r'\s*\((.*?)\)\s*;', types, re.S)
        # Ellipsis is intentionally outside the ordinary parameter parser.
        sig = found[1] if found else ''
        if name.endswith('_format'):
            if not sig.rstrip().endswith('...') or parameters(sig.rsplit(',', 1)[0]) != expected[:2]:
                raise ValueError('Adapt native formatting signature: ' + name)
        elif not found or parameters(sig) != expected:
            raise ValueError('Adapt native formatting signature: ' + name)
    h = ['/** Argument array for portable C89 64-bit and schema formatting. The',
         ' * formatter/parser is generated from the pinned native mp_printf source.',
         ' * Each * width/precision consumes INT. Types must match the native format',
         ' * (including integer length modifiers); mismatches return BADTYPEMISMATCH.',
         ' * Invalid arguments abort before padding or allocating the output.',
         ' * Referenced strings and schema byte storage borrow until return. */',
         '/** Native formatter argument discriminant. Select its corresponding public FormatArg union arm. */\ntypedef enum {']
    h += [f'  CPKT_OPCUA_FORMAT_{kind} = {i},' for i, (kind, _, _) in enumerate(ARGUMENTS.values())]
    h += ['  CPKT_OPCUA_FORMAT_KIND_COUNT', '} cpkt_opcua_FormatArgKind;',
          '/** One typed native-formatter argument. Select the matching union arm; strings/records and pointer arguments borrow for the formatting call. */\ntypedef struct { cpkt_opcua_FormatArgKind kind; union {']
    for spelling, (kind, member, _) in ARGUMENTS.items():
        public = {'long long': 'cpkt_opcua_Int64', 'unsigned long long': 'cpkt_opcua_UInt64',
                  'char *': 'const char *'}.get(spelling, spelling.replace('UA_', 'cpkt_opcua_'))
        h += [f'  {public} {member};']
    h += ['} value; } cpkt_opcua_FormatArg;',
          '/** Native format semantics, including %S/%N/%Q with C89 records by value.',
          ' * Ordinary C89 varargs use int/unsigned int/long/unsigned long/double,',
          ' * character/void pointers and the declared schema records. %ll consumes native C99',
          ' * long long as upstream: C89 callers use format_args for paired 64-bit',
          ' * values instead. Never pass a paired struct to the variadic %ll form.',
          ' * NULL-data/zero-length output allocates; nonzero length is capacity,',
          ' * including trailing NUL. Native truncation can write the buffer and',
          ' * return BADENCODINGLIMITSEXCEEDED, retaining its capacity. A zero-length',
          ' * result retains the provided pointer. Allocated outputs are owned.',
          ' * vformat borrows arguments during the call; end your own va_list. */',
          'cpkt_opcua_StatusCode cpkt_opcua_String_format(cpkt_opcua_String *str, const char *format, ...);',
          '/** Format native-style varargs into a String. C89 records use %S/%N/%Q; paired 64-bit values require format_args, never variadic %ll. */',
          'cpkt_opcua_StatusCode cpkt_opcua_String_vformat(cpkt_opcua_String *str, const char *format, va_list args);',
          '/** Format a typed argument array with explicit paired 64-bit values. Input values borrow through return; String capacity, ownership and truncation follow the native formatter. */',
          'cpkt_opcua_StatusCode cpkt_opcua_String_format_args(cpkt_opcua_String *str, const char *format, size_t count, const cpkt_opcua_FormatArg *args);']
    # No fallback local parser: unsupported/new upstream argument reads fail closed.
    text = (source / 'mp_printf.c').read_text()
    reads = re.findall(r'va_arg\(args,\s*(.*?)\)', text)
    if set(reads) != set(ARGUMENTS) or len(reads) != 19:
        raise ValueError('Adapt upstream formatter argument reads: ' + str(reads))
    text = text.replace('#include "mp_printf.h"', '#include <cpkt/opcua_types.h>\n#include <stdarg.h>\n#include <string.h>')
    dtoa = (source / 'dtoa.c').read_text().replace('#include "dtoa.h"', '')
    if dtoa.count('unsigned dtoa(double d, char* buffer)') != 1:
        raise ValueError('Adapt upstream dtoa declaration')
    dtoa = dtoa.replace('unsigned dtoa(double d, char* buffer)', 'static unsigned cpkt_format_dtoa(double d, char* buffer)')
    text = text.replace('#include "dtoa.h"', dtoa).replace('dtoa(value, buf)', 'cpkt_format_dtoa(value, buf)')
    # These are native arithmetic correctness fixes, not different formatting.
    text = text.replace('(_x) > 0 ? (_x) : -((printf_signed_value_t)_x)', '(_x) > 0 ? (printf_unsigned_value_t)(_x) : (0ULL - (printf_unsigned_value_t)(_x))')
    text = text.replace('width = (size_t)-w;', 'width = (size_t)(0U - (unsigned int)w);')
    text = text.replace('    char buf[PRINTF_DECIMAL_BUFFER_SIZE];', '    (void)precision; (void)width;\n    char buf[PRINTF_DECIMAL_BUFFER_SIZE];')
    text = text.replace('const char *format, va_list args)', 'const char *format, cpkt_format_reader *args)')
    text = re.sub(r'va_arg\(args,\s*(.*?)\)', lambda m: 'cpkt_format_read_' + ARGUMENTS[m[1]][2] + '(args)', text)
    end = text.index('\nint\nmp_vsnprintf(')
    if 'va_arg(' in text[:end] or text[end:].count('mp_snprintf(') != 1:
        raise ValueError('Adapt upstream formatter entry points')
    text = text[:end]
    prefix = ['/* Generated ABI argument bridge; native formatter follows below. */',
              '#include <cpkt/opcua_types.h>', '#include <open62541/types.h>',
              '#include <stdarg.h>', '#include <string.h>', '#include <setjmp.h>',
              'typedef struct { va_list va; const cpkt_opcua_FormatArg *values; size_t count, index; UA_StatusCode status; int typed; jmp_buf failure; } cpkt_format_reader;',
              '/* Argument readers allocate nothing. Abort invalid reads before native',
              ' * padding/printing; all native formatting of valid input is unchanged. */',
              'static void cpkt_format_fail(cpkt_format_reader *r, UA_StatusCode status) {',
              '  r->status = status; longjmp(r->failure, 1);', '}',
              'static const cpkt_opcua_FormatArg *cpkt_format_next(cpkt_format_reader *r, cpkt_opcua_FormatArgKind kind) {',
              '  if(r->index >= r->count || r->values[r->index].kind != kind) { cpkt_format_fail(r, UA_STATUSCODE_BADTYPEMISMATCH); return NULL; }',
              '  return &r->values[r->index++];', '}']
    for spelling, (kind, member, name) in ARGUMENTS.items():
        native_record = spelling.startswith('UA_')
        public = spelling.replace('UA_', 'cpkt_opcua_')
        prefix += [f'static {spelling} cpkt_format_read_{name}(cpkt_format_reader *r) {{',
                   '  const cpkt_opcua_FormatArg *v;']
        if native_record:
            prefix += [f'  {public} p;', f'  {spelling} out;',
                       '  memset(&out, 0, sizeof(out));',
                       f'  if(!r->typed) p = va_arg(r->va, {public});',
                       f'  else {{ v = cpkt_format_next(r, CPKT_OPCUA_FORMAT_{kind}); if(!v) return out; p = v->value.{member}; }}']
            if name == 'string':
                prefix += ['  out.length = p.length; out.data = p.data;',
                           '  if(out.length && (!out.data || out.data == UA_EMPTY_ARRAY_SENTINEL)) { cpkt_format_fail(r, UA_STATUSCODE_BADINVALIDARGUMENT); out = UA_STRING_NULL; }']
            elif name == 'qualifiedname':
                prefix += ['  out.namespaceIndex = p.namespaceIndex; out.name.length = p.name.length; out.name.data = p.name.data;',
                           '  if(out.name.length && (!out.name.data || out.name.data == UA_EMPTY_ARRAY_SENTINEL)) { cpkt_format_fail(r, UA_STATUSCODE_BADINVALIDARGUMENT); out.name = UA_STRING_NULL; }']
            else:
                prefix += ['  out.namespaceIndex = p.namespaceIndex; out.identifierType = (enum UA_NodeIdType)p.identifierType;',
                           '  switch(out.identifierType) {',
                           '  case UA_NODEIDTYPE_NUMERIC: out.identifier.numeric = p.identifier.numeric; break;',
                           '  case UA_NODEIDTYPE_GUID: memcpy(&out.identifier.guid, &p.identifier.guid, sizeof(out.identifier.guid)); break;',
                           '  case UA_NODEIDTYPE_STRING: case UA_NODEIDTYPE_BYTESTRING:',
                           '    out.identifier.string.length = p.identifier.string.length; out.identifier.string.data = p.identifier.string.data;',
                           '    if(out.identifier.string.length && (!out.identifier.string.data || out.identifier.string.data == UA_EMPTY_ARRAY_SENTINEL)) { cpkt_format_fail(r, UA_STATUSCODE_BADINVALIDARGUMENT); out = UA_NODEID_NULL; } break;',
                           '  default: cpkt_format_fail(r, UA_STATUSCODE_BADINVALIDARGUMENT); out = UA_NODEID_NULL; break;', '  }']
            prefix += ['  return out;', '}']
        elif name in ('int64', 'uint64'):
            prefix += ['  UA_UInt64 bits;',
                       f'  if(!r->typed) return va_arg(r->va, {spelling});',
                       f'  v = cpkt_format_next(r, CPKT_OPCUA_FORMAT_{kind}); if(!v) return 0;',
                       f'  bits = ((UA_UInt64)v->value.{member}.high32 << 32) | v->value.{member}.low32;']
            if name == 'int64':
                prefix += ['  UA_Int64 signed_bits; memcpy(&signed_bits, &bits, sizeof(bits)); return signed_bits;']
            else:
                prefix += ['  return bits;']
            prefix += ['}']
        else:
            prefix += [f'  if(!r->typed) return va_arg(r->va, {spelling});',
                       f'  v = cpkt_format_next(r, CPKT_OPCUA_FORMAT_{kind});',
                       f'  return v ? ({spelling})v->value.{member} : 0;', '}']
    (Path(output) / 'opcua_native_format.c').write_text('\n'.join(prefix) + '\n' + text + '\n' + ENTRY)
    return h


ENTRY = r'''
/* Optimized native formatter inlining can reuse these pointer arguments.
 * Keep their values defined across the jump boundary without disabling warnings.
 * The error branch returns immediately; only the caller-owned reader survives. */
static int cpkt_format_run(char *volatile s, size_t n, const char *volatile format, cpkt_format_reader *r) {
    output_t out = {s, 0, n};
    if(setjmp(r->failure)) return 0;
    format_string_loop(&out, format, r);
    if(n > 1) {
        size_t null_char_pos = out.pos < n ? out.pos : n - 1;
        out.buffer[null_char_pos] = '\0';
    }
    return (int)out.pos;
}
static UA_StatusCode cpkt_format_string(cpkt_opcua_String *str, const char *format,
                                       cpkt_format_reader *reader) {
    cpkt_format_reader second = *reader;
    if(!reader->typed) va_copy(second.va, reader->va);
    int out = cpkt_format_run((char*)str->data, str->length, format, reader);
    UA_StatusCode status = reader->status;
    if(status) goto done;
    if(out < 0) { status = UA_STATUSCODE_BADENCODINGERROR; goto done; }
    if(out == 0) {
        str->length = 0;
        if(!str->data) str->data = (cpkt_opcua_Byte*)UA_EMPTY_ARRAY_SENTINEL;
        goto done;
    }
    if(str->length > 0) {
        if((size_t)out < str->length) str->length = (size_t)out;
        else status = UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
        goto done;
    }
    UA_ByteString native = UA_STRING_NULL;
    status = UA_ByteString_allocBuffer(&native, (size_t)out + 1);
    if(status) goto done;
    cpkt_format_run((char*)native.data, native.length, format, &second);
    if(second.status) { UA_ByteString_clear(&native); status = second.status; goto done; }
    str->data = native.data;
    str->length = native.length - 1;
 done:
    if(!reader->typed) va_end(second.va);
    return status;
}
cpkt_opcua_StatusCode cpkt_opcua_String_vformat(cpkt_opcua_String *str, const char *format, va_list args) {
    if(!str || !format || (str->length && (!str->data || str->data == UA_EMPTY_ARRAY_SENTINEL)))
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    cpkt_format_reader r;
    memset(&r, 0, sizeof(r));
    va_copy(r.va, args);
    UA_StatusCode status = cpkt_format_string(str, format, &r);
    va_end(r.va);
    return status;
}
cpkt_opcua_StatusCode cpkt_opcua_String_format(cpkt_opcua_String *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    UA_StatusCode status = cpkt_opcua_String_vformat(str, format, args);
    va_end(args);
    return status;
}
cpkt_opcua_StatusCode cpkt_opcua_String_format_args(cpkt_opcua_String *str, const char *format,
                                                   size_t count, const cpkt_opcua_FormatArg *args) {
    if(!str || !format || (count && !args) ||
       (str->length && (!str->data || str->data == UA_EMPTY_ARRAY_SENTINEL)))
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    cpkt_format_reader r;
    memset(&r, 0, sizeof(r));
    r.typed = 1; r.values = args; r.count = count;
    return cpkt_format_string(str, format, &r);
}
'''


def emit_logger(headers):
    text = uncomment((headers / 'plugin/log.h').read_text())
    record = re.search(r'typedef\s+struct\s+UA_Logger\s*\{(.*?)\}\s*UA_Logger\s*;', text, re.S)
    expected = ('void (*log)(void *logContext, UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args); '
                'void *context; void (*clear)(struct UA_Logger *logger);')
    if not record or ' '.join(record[1].split()) != expected:
        raise ValueError('Adapt complete public native logger record')
    h = [re.match(r'/\*.*?\*/', (headers / 'plugin/log.h').read_text(), re.S)[0],
         '/** Complete C89 logger plugin callbacks. Native messages are rendered one',
         ' * at a time, then delivered as %S with a C89 String by value; use the',
         ' * C89 String_vformat if decoding arguments. Native format varargs never',
         ' * cross the C89 schema boundary unchanged. Message storage borrows during',
         ' * log only. clear owns context according to your plugin and is called once',
         ' * on replacement/destruction. Copied records alias one context. */',
         'typedef struct cpkt_opcua_Logger {',
         '  void (*log)(void *logContext, cpkt_opcua_log_level level, cpkt_opcua_log_category category, const char *msg, va_list args);',
         '  void *context;', '  void (*clear)(struct cpkt_opcua_Logger *logger);',
         '} cpkt_opcua_Logger;',
         '/** Native stock backends; formatting uses C89 arguments before passing one',
         ' * rendered message to the unchanged backend. Native timestamp/filtering',
         ' * and output/truncation behavior stay native. withLevel returns a borrowed',
         ' * context with NULL clear. new owns one native logger and C89 root; clear',
         ' * exactly once. Copies alias that ownership. Syslog requires openlog(3). */',
         'extern const cpkt_opcua_Logger cpkt_opcua_Log_Stdout_;',
         'extern const cpkt_opcua_Logger *cpkt_opcua_Log_Stdout;',
         '/** Use the native-backed logger with C89 message arguments. Message bytes borrow until callback return; owned heap loggers are cleared once and configuration views must not be independently cleared. */\ncpkt_opcua_Logger cpkt_opcua_Log_Stdout_withLevel(cpkt_opcua_log_level minlevel);',
         '/** Use the native-backed logger with C89 message arguments. Message bytes borrow until callback return; owned heap loggers are cleared once and configuration views must not be independently cleared. */\ncpkt_opcua_Logger *cpkt_opcua_Log_Stdout_new(cpkt_opcua_log_level minlevel);',
         '#if defined(__linux__) || defined(__unix__)',
         '/** Use the native-backed logger with C89 message arguments. Message bytes borrow until callback return; owned heap loggers are cleared once and configuration views must not be independently cleared. */\ncpkt_opcua_Logger cpkt_opcua_Log_Syslog(void);',
         '/** Use the native-backed logger with C89 message arguments. Message bytes borrow until callback return; owned heap loggers are cleared once and configuration views must not be independently cleared. */\ncpkt_opcua_Logger cpkt_opcua_Log_Syslog_withLevel(cpkt_opcua_log_level minlevel);',
         '/** Use the native-backed logger with C89 message arguments. Message bytes borrow until callback return; owned heap loggers are cleared once and configuration views must not be independently cleared. */\ncpkt_opcua_Logger *cpkt_opcua_Log_Syslog_new(cpkt_opcua_log_level minlevel);', '#endif',
         '/** Native level helpers, respecting configured UA_LOGLEVEL. These invoke',
         ' * the C89 plugin directly with C89 arguments and borrow through return. */']
    for name in ('TRACE','DEBUG','INFO','WARNING','ERROR','FATAL'):
        h += ['/** Deliver one C89-formatted log message at this severity; arguments and message bytes borrow until return. A NULL logger is harmless. */\n' f'void cpkt_opcua_LOG_{name}(const cpkt_opcua_Logger *logger, cpkt_opcua_log_category category, const char *msg, ...);']
    for kind in ('Client','Server'):
        h += [f'/** Replace logging callbacks in place, retaining the native logger address',
              ' * borrowed by event loops and security plugins. Success takes ownership',
              ' * of context/clear and zeroes source; failure preserves source. NULL',
              ' * silences logging. User callbacks must not destroy their configuration.',
              ' * Getter returns borrowed callable slots for the current native logger;',
              ' * its clear is NULL because configuration owns backend destruction.',
              ' * Serialize all calls with configuration and logger use. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_setLoggerPlugin(cpkt_opcua_{kind}Config *config, cpkt_opcua_Logger *logger);',
              '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_getLoggerPlugin(cpkt_opcua_{kind}Config *config, cpkt_opcua_Logger *logger);']
    return h
