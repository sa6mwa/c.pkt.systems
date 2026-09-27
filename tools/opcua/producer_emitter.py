"""Derive public producer callback declarations from installed native headers."""
import re
from node_emitter import record_body
from plugin_emitter import callbacks, parameters, translate, validate_fields


def emit_producers(index, native_headers):
    text = (native_headers / 'server.h').read_text()
    body = record_body(text, 'CallbackValueSource')
    validate_fields(body, [])
    slots = callbacks(body)
    if tuple(name for _, name, _ in slots) != ('read', 'write'):
        raise ValueError('Adapt public value-source slots')
    method = re.search(r'typedef\s+UA_StatusCode\s*\(\*UA_MethodCallback\)\s*\((.*?)\)\s*;', text, re.S)
    if not method:
        raise ValueError('Adapt public method callback declaration')
    signatures = slots + [('UA_StatusCode', 'method', method[1])]
    expected = {
        'read': ['UA_Server *', 'const UA_NodeId *', 'void *', 'const UA_NodeId *', 'void *', 'UA_Boolean', 'const UA_NumericRange *', 'UA_DataValue *'],
        'write': ['UA_Server *', 'const UA_NodeId *', 'void *', 'const UA_NodeId *', 'void *', 'const UA_NumericRange *', 'const UA_DataValue *'],
        'method': ['UA_Server *', 'const UA_NodeId *', 'void *', 'const UA_NodeId *', 'void *', 'const UA_NodeId *', 'void *', 'size_t', 'const UA_Variant *', 'size_t', 'UA_Variant *'],
    }
    metadata = []
    for result, name, signature in signatures:
        args = parameters(signature)
        if result != 'UA_StatusCode' or [spelling for spelling, _ in args] != expected[name]:
            raise ValueError(f'Adapt public producer signature: {name}')
        metadata += [f'static UA_StatusCode cpkt_producer_native_{name}({signature}) {{',
                     f'  return cpkt_producer_{name}({", ".join(param for _, param in args)});', '}']
    header = [re.match(r'/\*.*?\*/', text, re.S)[0],
              '/** Full native value-source callbacks with original contexts. NodeIds/ranges',
              ' * are borrowed until return. Write values are borrowed and must not be cleared.',
              ' * Read outputs own C89 allocations. Return GoodCompletesAsynchronously to',
              ' * retain the value address until native completion/cancellation; no input',
              ' * NodeId or range may be retained. Serialize all calls on the server.',
              ' * Normal outputs are converted once; NODELETE payloads remain caller-owned.',
              ' * For native borrowing use valueSourceBorrow_typed with persistent storage.',
              ' * Initial async conversion failure invalidates the address via cancellation;',
              ' * the operation reports the conversion status instead of queuing. */',
              'typedef struct {', translate(body), '} cpkt_opcua_CallbackValueSource;',
              '/** Full method callback. Input array is borrowed until return. Output array',
              ' * slots own C89 allocations; count is fixed by the native method metadata.',
              ' * GoodCompletesAsynchronously retains the output address until completion or',
              ' * cancellation, including zero-length output arrays. Contexts are unchanged.',
              ' * Initial async conversion failure issues cancellation and reports its status. */',
              'typedef cpkt_opcua_StatusCode (*cpkt_opcua_MethodCallback)(' + translate(method[1]) + ');',
              '/** Replace copied value-source slots; failure preserves the old registration.',
              ' * Contexts stay caller-owned. NULL write retains native read-only behavior.',
              ' * In-flight callbacks may replace their own registration. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_setVariableNode_callbackValueSource_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId, cpkt_opcua_CallbackValueSource source);',
              '/** Replace the method callback without changing method/node/session contexts.',
              ' * NULL disables it. Failure preserves the old registration. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_setMethodNodeCallback_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId, cpkt_opcua_MethodCallback method);',
              '/** Get the original typed callback or NULL. Native callbacks installed outside',
              ' * this full C89 interface return BadNotSupported, never an incompatible cast. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_getMethodNodeCallback_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId, cpkt_opcua_MethodCallback *method);',
              '/** Set the native cancellation hook before startup. Own producer operations',
              ' * use their original C89 value/output address, valid only during this callback.',
              ' * Do not clear or complete it. Other native operations forward their opaque',
              ' * address. Any pre-existing native hook also receives the native address.',
              ' * NULL disables user notification; required facade cleanup remains installed. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_set_async_operation_cancel_callback_typed(cpkt_opcua_server *server, void (*callback)(cpkt_opcua_server *, const void *));',
              '/** Synchronize native post-read timestamp metadata for newly pending outputs.',
              ' * Typed submissions and server iteration do this automatically. Native escape',
              ' * hatch event-loop callers can invoke this after native processing returns.',
              ' * This runs once per result, leaving subsequent application edits intact. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_refresh_async_producer_metadata(cpkt_opcua_server *server);',
              '/** Complete the original read output after its producer callback returned.',
              ' * Good consumes the result address; do not access it afterward. Conversion',
              ' * failure retains the pending result for retry; unknown/active addresses',
              ' * return BadNotFound. Native queue processing and timeouts are unchanged. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_setAsyncReadResult_typed(cpkt_opcua_server *server, cpkt_opcua_DataValue *result);',
              '/** Complete the original borrowed write value with the native status. Good',
              ' * invalidates the address; cancellation also invalidates it. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_setAsyncWriteResult_typed(cpkt_opcua_server *server, const cpkt_opcua_DataValue *value, cpkt_opcua_StatusCode result);',
              '/** Complete the original method output array and native status. Stages every',
              ' * output before changing native storage; conversion failure allows retry.',
              ' * Good invalidates the whole array. Do not resize its fixed output count. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_setAsyncCallMethodResult_typed(cpkt_opcua_server *server, cpkt_opcua_Variant *output, cpkt_opcua_StatusCode result);',
              '/** Borrow stable native DataValue storage for a read producer result. The',
              ' * existing history_value holder owns that storage; this does not copy its',
              ' * payload. Native Variant becomes NODELETE. Keep the holder unchanged/alive',
              ' * through all native use, including encoding or the local result callback.',
              ' * Clear/quiesce all borrowers before set/free. NULL resumes normal C89 output',
              ' * conversion. Other C89 result fields are ignored while a holder is selected. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_valueSourceBorrow_typed(cpkt_opcua_server *server, cpkt_opcua_DataValue *result, const cpkt_opcua_history_value *stored);',
              '/** Borrow a persistent native Variant for one method output slot. The holder',
              ' * and its payload/dimensions must outlive all native uses; quiesce before',
              ' * set/free. This performs no payload copy. NULL resumes C89 conversion for',
              ' * that slot. Other C89 slot fields are ignored while selected. Allocation',
              ' * failure preserves selections. Valid during the callback or while pending. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_methodResultBorrow_typed(cpkt_opcua_server *server, cpkt_opcua_Variant *outputs, size_t index, const cpkt_opcua_history_value *stored);']
    from creation_emitter import emit_creation
    creation_header, creation_metadata = emit_creation(index, native_headers)
    header += creation_header
    metadata += creation_metadata
    return header, metadata
