"""Expose native event-loop methods without copying architecture-private state.

The upstream public declarations remain the inventory. Callback context/byte
ownership is binding policy; polling, timers and transport remain upstream.
"""
import re
from core_client_emitter import enum_declaration
from plugin_emitter import public_body, callbacks, validate_fields, uncomment, parameters


def emit_eventloop(headers, output):
    path = headers / 'plugin/eventloop.h'
    text = path.read_text()
    config = (headers / 'config.h').read_text()
    expected = {
        'EventLoop': (['const UA_Logger *logger', 'UA_KeyValueMap params',
                       'const volatile UA_EventLoopState state', 'UA_EventSource *eventSources'],
                      {'start', 'stop', 'free', 'run', 'cancel', 'dateTime_now',
                       'dateTime_nowMonotonic', 'dateTime_localTimeUtcOffset',
                       'nextTimer', 'addTimer', 'modifyTimer', 'removeTimer',
                       'addDelayedCallback', 'removeDelayedCallback',
                       'registerEventSource', 'deregisterEventSource', 'lock', 'unlock'}),
        'EventSource': (['struct UA_EventSource *next', 'UA_EventSourceType eventSourceType',
                         'UA_String name', 'UA_EventLoop *eventLoop', 'UA_KeyValueMap params',
                         'UA_EventSourceState state'], {'start', 'stop', 'free'}),
        'ConnectionManager': (['UA_EventSource eventSource', 'UA_String protocol'],
                              {'openConnection', 'sendWithConnection', 'closeConnection',
                               'allocNetworkBuffer', 'freeNetworkBuffer'}),
        'InterruptManager': (['UA_EventSource eventSource'],
                             {'registerInterrupt', 'deregisterInterrupt'}),
    }
    for name, (fields, methods) in expected.items():
        body = public_body(path, name, config)
        validate_fields(body, fields)
        if {m for _, m, _ in callbacks(body)} != methods:
            raise ValueError('Adapt all public event-loop methods: ' + name)
    for factory in ('EventLoop_new_POSIX', 'ConnectionManager_new_POSIX_TCP',
                    'ConnectionManager_new_POSIX_UDP', 'ConnectionManager_new_POSIX_Ethernet',
                    'ConnectionManager_new_MQTT', 'InterruptManager_new_POSIX'):
        if not re.search(r'UA_' + factory + r'\s*\(', uncomment(text)):
            raise ValueError('Adapt event-loop factory: ' + factory)
    declaration, _ = enum_declaration((headers / 'common.h').read_text(), 'ConnectionState')
    header = ['/** Native connection lifecycle state; callbacks preserve upstream transitions. */', declaration]
    for name in ('TimerPolicy', 'EventLoopState', 'EventSourceState', 'EventSourceType'):
        declaration, _ = enum_declaration(text, name)
        header += ['/** Complete upstream event-loop enum. */', declaration]
    header += [
        '/** Native architecture backends. Their private socket/timer state stays native.',
        ' * Serialize operations on a loop and its sources, including callbacks. Do not',
        ' * free a loop/source from a callback. Native stop is asynchronous: continue run',
        ' * until STOPPED before free. A registered source is owned by the loop; after',
        ' * successful deregistration it becomes caller-owned again. A failed register',
        ' * can still attach a source if its native start failed; inspect eventLoop.',
        ' * These methods expose native scheduling; no facade queue or polling exists. */',
        'typedef struct cpkt_opcua_EventLoop cpkt_opcua_EventLoop;',
        '/** Borrow the actual configuration event loop. Native configuration',
        ' * destruction also releases facade metadata. An external loop remains',
        ' * caller-owned; an internal loop must not be freed independently.',
        ' * Output is NULL on failure or if the configuration has no event loop.',
        ' * Serialize this lookup with configuration and loop operations. */',
        'cpkt_opcua_StatusCode cpkt_opcua_client_get_event_loop_typed(cpkt_opcua_client *client, cpkt_opcua_EventLoop **loop);',
        '/** Borrow the actual configuration EventLoop. Internal loops belong to configuration; external loops remain caller-owned. Serialize lookup and loop/configuration changes. */\ncpkt_opcua_StatusCode cpkt_opcua_server_get_event_loop_typed(cpkt_opcua_server *server, cpkt_opcua_EventLoop **loop);',
        '/** Opaque actual native EventSource storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_EventSource cpkt_opcua_EventSource;',
        '/** Opaque actual native ConnectionManager storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_ConnectionManager cpkt_opcua_ConnectionManager;',
        '/** Opaque actual native InterruptManager storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_InterruptManager cpkt_opcua_InterruptManager;',
        '/** Native callback signature. Application/context pointers remain caller-owned; arguments borrow until return. */\ntypedef void (*cpkt_opcua_Callback)(void *application, void *context);',
        '/** Native delayed callback backing. Opaque storage allows the native queue',
        ' * to link the actual record, without allocation during add or type-punning',
        ' * C89 records. The owned factory is fallible; add/remove retain native',
        ' * thread-safety. Native callback/application/context have the same C89 ABI. */',
        'typedef struct cpkt_opcua_DelayedCallback cpkt_opcua_DelayedCallback;',
        '/** Complete delayed callback fields. next is a borrowed scheduling link;',
        ' * only the native loop changes it. Callback may delete its own record.',
        ' * Do not delete a queued record before remove or execution. */',
        'typedef struct { cpkt_opcua_DelayedCallback *next; cpkt_opcua_Callback callback; void *application; void *context; } cpkt_opcua_DelayedCallbackInfo;',
        '/** Allocate owned native delayed-callback backing with borrowed application/context. NULL reports allocation failure; keep the record alive while queued. */\ncpkt_opcua_DelayedCallback *cpkt_opcua_DelayedCallback_new(cpkt_opcua_Callback callback, void *application, void *context);',
        '/** Release delayed-callback backing only after removal or execution. The callback may delete its own dequeued record. */\nvoid cpkt_opcua_DelayedCallback_delete(cpkt_opcua_DelayedCallback *callback);',
        '/** Inspect the actual delayed record. Callback/context are borrowed and next is a native scheduling link; serialize with dispatch. */\ncpkt_opcua_StatusCode cpkt_opcua_DelayedCallback_getInfo(const cpkt_opcua_DelayedCallback *callback, cpkt_opcua_DelayedCallbackInfo *info);',
        '/** Change callback/application/context on a quiescent record. next is ignored.',
        ' * Serialize changes with dispatch. No copy of application/context is made. */',
        'cpkt_opcua_StatusCode cpkt_opcua_DelayedCallback_setInfo(cpkt_opcua_DelayedCallback *callback, const cpkt_opcua_DelayedCallbackInfo *info);',
        '/** Complete source metadata. name/protocol borrow native bytes; never clear',
        ' * them. eventLoop/next borrow handles owned with their native objects. */',
        'typedef struct { cpkt_opcua_EventSourceType eventSourceType; cpkt_opcua_EventSourceState state; cpkt_opcua_String name; cpkt_opcua_EventLoop *eventLoop; cpkt_opcua_EventSource *next; } cpkt_opcua_EventSourceInfo;',
        '/** Native connection dispatch. Pointer-width IDs use size_t on all SDK',
        ' * targets. Payload bytes borrow the actual native receive buffer until',
        ' * return: no message copy or materialization. Parameters borrow until return.',
        ' * Conversion failure is explicit (params is NULL). connectionContext is the',
        ' * original caller slot and may be replaced; accepted connections inherit the',
        ' * current listening context. Final CLOSING releases the dispatch registration.',
        ' * Close from the callback is supported; do not destroy its manager/loop. */',
        'typedef void (*cpkt_opcua_ConnectionManager_connectionCallback)(cpkt_opcua_ConnectionManager *manager, size_t connectionId, void *application, void **connectionContext, cpkt_opcua_ConnectionState state, cpkt_opcua_StatusCode conversionStatus, const cpkt_opcua_KeyValueMap *params, cpkt_opcua_ByteString message);',
        '/** Native interrupt dispatch and original context. instanceInfos borrows',
        ' * until return and is NULL on conversion failure. The callback may deregister',
        ' * itself. Native interruptHandle is forwarded, even where the platform uses',
        ' * a dispatch handle distinct from the registration signal number. */',
        'typedef void (*cpkt_opcua_InterruptCallback)(cpkt_opcua_InterruptManager *manager, size_t interruptHandle, void *context, cpkt_opcua_StatusCode conversionStatus, const cpkt_opcua_KeyValueMap *instanceInfos);',
        '/** Create the native POSIX loop. Logger configuration is copied; user data',
        ' * borrows through loop destruction. NULL deliberately silences its logger. */',
        'cpkt_opcua_EventLoop *cpkt_opcua_EventLoop_new_POSIX(const cpkt_opcua_log_config *logger);',
        '/** Replace logging without moving the logger address borrowed by sources. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventLoop_setLogger(cpkt_opcua_EventLoop *loop, const cpkt_opcua_log_config *logger);',
        '/** Copy the complete native parameter map; output must start empty and owns',
        ' * every nested value on success. clear with KeyValueMap_clear. Set copies',
        ' * the input before replacing native owned storage; use before native start. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventLoop_getParams(cpkt_opcua_EventLoop *loop, cpkt_opcua_KeyValueMap *params);',
        '/** Copy parameters before replacing native owned loop storage. Use while quiescent before native start; failures preserve the old map. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_setParams(cpkt_opcua_EventLoop *loop, const cpkt_opcua_KeyValueMap *params);',
        '/** Inspect actual native loop state. No snapshot of scheduler/socket state is made; serialize with loop operations. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_getState(cpkt_opcua_EventLoop *loop, cpkt_opcua_EventLoopState *state);',
        '/** Native start/free status. free consumes the handle only on Good. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventLoop_start(cpkt_opcua_EventLoop *loop);',
        '/** Free the stopped native loop. Good consumes the handle and registered sources; failure retains it for retry. Never free from a callback. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_free(cpkt_opcua_EventLoop *loop);',
        '/** Run the native loop for timeout milliseconds. Dispatch and transport buffers remain native; callbacks must not destroy the loop. Continue run after stop until STOPPED. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_run(cpkt_opcua_EventLoop *loop, cpkt_opcua_UInt32 timeout);',
    ]
    for method in ('stop', 'cancel', 'lock', 'unlock'):
        header += [f'/** Invoke the native {method} method; NULL is harmless. */',
                   f'void cpkt_opcua_EventLoop_{method}(cpkt_opcua_EventLoop *loop);']
    for method, typename in (('dateTime_now', 'DateTime'), ('dateTime_nowMonotonic', 'DateTime'),
                             ('dateTime_localTimeUtcOffset', 'Int64'), ('nextTimer', 'DateTime')):
        header += ['/** Native time domain with every 64-bit bit retained; NULL returns zero. */',
                   '/** Free the stopped native loop. Good consumes the handle and registered sources; failure retains it for retry. Never free from a callback. */\n' f'cpkt_opcua_{typename} cpkt_opcua_EventLoop_{method}(cpkt_opcua_EventLoop *loop);']
    header += [
        '/** Native timer submission. Inputs borrow until return; callback/context',
        ' * borrow until removal or execution (ONCE). Optional baseTime/timerId retain',
        ' * native meaning and all 64 bits. Callback may modify/remove its own timer. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventLoop_addTimer(cpkt_opcua_EventLoop *loop, cpkt_opcua_Callback callback, void *application, void *context, cpkt_opcua_Double interval_ms, cpkt_opcua_DateTime *baseTime, cpkt_opcua_TimerPolicy timerPolicy, cpkt_opcua_UInt64 *timerId);',
        '/** Modify/remove the native timer using all 64 ID bits. Callback/application/context borrow until dispatch or removal; self-removal is supported. Serialize loop changes. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_modifyTimer(cpkt_opcua_EventLoop *loop, cpkt_opcua_UInt64 timerId, cpkt_opcua_Double interval_ms, cpkt_opcua_DateTime *baseTime, cpkt_opcua_TimerPolicy timerPolicy);',
        '/** Modify/remove the native timer using all 64 ID bits. Callback/application/context borrow until dispatch or removal; self-removal is supported. Serialize loop changes. */\nvoid cpkt_opcua_EventLoop_removeTimer(cpkt_opcua_EventLoop *loop, cpkt_opcua_UInt64 timerId);',
        '/** Add/remove the actual delayed record in the native queue. No payload copy or new queue is introduced; remove a queued record before deleting its backing. */\nvoid cpkt_opcua_EventLoop_addDelayedCallback(cpkt_opcua_EventLoop *loop, cpkt_opcua_DelayedCallback *callback);',
        '/** Add/remove the actual delayed record in the native queue. No payload copy or new queue is introduced; remove a queued record before deleting its backing. */\nvoid cpkt_opcua_EventLoop_removeDelayedCallback(cpkt_opcua_EventLoop *loop, cpkt_opcua_DelayedCallback *callback);',
        '/** Register/deregister the actual native source. The loop owns attached sources; successful deregistration returns ownership. Failed start may still attach: inspect source.eventLoop. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_registerEventSource(cpkt_opcua_EventLoop *loop, cpkt_opcua_EventSource *source);',
        '/** Register/deregister the actual native source. The loop owns attached sources; successful deregistration returns ownership. Failed start may still attach: inspect source.eventLoop. */\ncpkt_opcua_StatusCode cpkt_opcua_EventLoop_deregisterEventSource(cpkt_opcua_EventLoop *loop, cpkt_opcua_EventSource *source);',
        '/** Borrow the first registered source; use getInfo.next to traverse. */',
        'cpkt_opcua_EventSource *cpkt_opcua_EventLoop_eventSources(cpkt_opcua_EventLoop *loop);',
        '/** Invoke/inspect the actual native source. Serialize with its loop; start/stop state and asynchronous completion retain upstream semantics. */\ncpkt_opcua_StatusCode cpkt_opcua_EventSource_getInfo(cpkt_opcua_EventSource *source, cpkt_opcua_EventSourceInfo *info);',
        '/** Copy a new native-owned source name; use before source start. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventSource_setName(cpkt_opcua_EventSource *source, cpkt_opcua_String name);',
        '/** Copy native source parameters into an empty owned output. Clear with KeyValueMap_clear; handle metadata remains native. */\ncpkt_opcua_StatusCode cpkt_opcua_EventSource_getParams(cpkt_opcua_EventSource *source, cpkt_opcua_KeyValueMap *params);',
        '/** Copy parameters into the quiescent native source before start. The input borrows through return; failure preserves prior parameters. */\ncpkt_opcua_StatusCode cpkt_opcua_EventSource_setParams(cpkt_opcua_EventSource *source, const cpkt_opcua_KeyValueMap *params);',
        '/** Invoke/inspect the actual native source. Serialize with its loop; start/stop state and asynchronous completion retain upstream semantics. */\ncpkt_opcua_StatusCode cpkt_opcua_EventSource_start(cpkt_opcua_EventSource *source);',
        '/** Invoke/inspect the actual native source. Serialize with its loop; start/stop state and asynchronous completion retain upstream semantics. */\nvoid cpkt_opcua_EventSource_stop(cpkt_opcua_EventSource *source);',
        '/** A registered source must first be stopped/deregistered. free consumes',
        ' * the caller-owned handle on native Good; failure preserves it. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventSource_free(cpkt_opcua_EventSource *source);',
    ]
    for name in ('POSIX_TCP', 'POSIX_UDP', 'POSIX_Ethernet', 'MQTT'):
        if name == 'POSIX_Ethernet':
            header += ['#if defined(__linux__)']
        header += ['/** Create the native manager; name bytes borrow during construction.',
                   ' * The returned source is caller-owned until registered with a loop. */',
                   f'cpkt_opcua_ConnectionManager *cpkt_opcua_ConnectionManager_new_{name}(cpkt_opcua_String name);']
        if name == 'POSIX_Ethernet':
            header += ['#endif']
    header += [
        '/** Allocate the native POSIX interrupt manager. The returned source is caller-owned until registration with a loop. On platforms without epoll, including macOS, the stock backend permits one manager per process and a second factory call returns NULL until the first is freed. */\ncpkt_opcua_InterruptManager *cpkt_opcua_InterruptManager_new_POSIX(cpkt_opcua_String name);',
        '/** Borrow the common EventSource portion; no allocation or ownership change. */',
        'cpkt_opcua_EventSource *cpkt_opcua_ConnectionManager_eventSource(cpkt_opcua_ConnectionManager *manager);',
        '/** Borrow the embedded native EventSource; lifetime and ownership follow the interrupt manager. */\ncpkt_opcua_EventSource *cpkt_opcua_InterruptManager_eventSource(cpkt_opcua_InterruptManager *manager);',
        '/** Invoke/inspect the actual native source. Serialize with its loop; start/stop state and asynchronous completion retain upstream semantics. */\ncpkt_opcua_ConnectionManager *cpkt_opcua_EventSource_connectionManager(cpkt_opcua_EventSource *source);',
        '/** Invoke/inspect the actual native source. Serialize with its loop; start/stop state and asynchronous completion retain upstream semantics. */\ncpkt_opcua_InterruptManager *cpkt_opcua_EventSource_interruptManager(cpkt_opcua_EventSource *source);',
        '/** Borrow the native protocol bytes; never clear or retain after free. */',
        'cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_protocol(cpkt_opcua_ConnectionManager *manager, cpkt_opcua_String *protocol);',
        '/** Native submission; may synchronously announce multiple connections.',
        ' * Params borrow during submission. Context/application/callback borrow through',
        ' * the final CLOSING delivery for every announced connection. Native failures',
        ' * and partial connection creation are preserved; no rollback is added. */',
        'cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_openConnection(cpkt_opcua_ConnectionManager *manager, const cpkt_opcua_KeyValueMap *params, void *application, void *context, cpkt_opcua_ConnectionManager_connectionCallback callback);',
        '/** Network buffers belong to this manager/connection. Allocation output starts',
        ' * empty. send consumes the actual native buffer even on native failure, exactly',
        ' * as upstream; pre-submission parameter conversion failure leaves it untouched.',
        ' * Do not release a network buffer using ordinary ByteString_clear. */',
        'cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_allocNetworkBuffer(cpkt_opcua_ConnectionManager *manager, size_t connectionId, cpkt_opcua_ByteString *buffer, size_t size);',
        '/** Release an unsent native network buffer through its originating manager; the record is reset. Do not free a buffer already consumed by send. */\nvoid cpkt_opcua_ConnectionManager_freeNetworkBuffer(cpkt_opcua_ConnectionManager *manager, size_t connectionId, cpkt_opcua_ByteString *buffer);',
        '/** Invoke native connection dispatch with pointer-width IDs and borrowed parameters. Receive bytes borrow until callback return; native close and context-slot ownership are preserved. */\ncpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_sendWithConnection(cpkt_opcua_ConnectionManager *manager, size_t connectionId, const cpkt_opcua_KeyValueMap *params, cpkt_opcua_ByteString *buffer);',
        '/** Invoke native connection dispatch with pointer-width IDs and borrowed parameters. Receive bytes borrow until callback return; native close and context-slot ownership are preserved. */\ncpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_closeConnection(cpkt_opcua_ConnectionManager *manager, size_t connectionId);',
        '/** Invoke native interrupt registration/removal. User context borrows until deregistration; a callback may deregister itself. Native dispatch handles and status are preserved. */\ncpkt_opcua_StatusCode cpkt_opcua_InterruptManager_registerInterrupt(cpkt_opcua_InterruptManager *manager, size_t interruptHandle, const cpkt_opcua_KeyValueMap *params, cpkt_opcua_InterruptCallback callback, void *context);',
        '/** Invoke native interrupt registration/removal. User context borrows until deregistration; a callback may deregister itself. Native dispatch handles and status are preserved. */\nvoid cpkt_opcua_InterruptManager_deregisterInterrupt(cpkt_opcua_InterruptManager *manager, size_t interruptHandle);',
    ]
    for kind in ('EventSource', 'ConnectionManager', 'InterruptManager'):
        body = public_body(path, kind, config)
        declaration = []
        if kind != 'EventSource':
            declaration += ['  cpkt_opcua_EventSourcePlugin eventSource;']
        for result, name, signature in callbacks(body):
            signature = signature.replace('UA_', 'cpkt_opcua_').replace('uintptr_t', 'size_t')
            declaration += [f'  {result.replace("UA_", "cpkt_opcua_")} (*{name})({signature});']
        header += [
            '/** Complete custom method table derived from the native public plugin.',
            ' * Tables are copied; backend context remains opaque. A successful free',
            ' * releases context, never the facade handle. Configure source metadata',
            ' * through its accessors. Buffer and mutable callback-context ownership',
            ' * follow native rules; no alternative transport or interrupt is supplied.',
            ' * Native callback conversion failure is logged: connections are closed',
            ' * and failing interrupt deliveries are skipped. Initial connection',
            ' * announcements must occur within openConnection, as required by native.',
            ' * Buffer send must consume storage even on failure. Context ownership',
            ' * transfers only on successful construction. */',
            'typedef struct {', *declaration, '} cpkt_opcua_' + kind + 'Plugin;']
    header += [
        '/** Install a callable C89 backend. Application/context passed to its openConnection are opaque forwarding closures; preserve them and the mutable context slot through every delivery, including final CLOSING. The user callback receives its original application and persistent mutable user context. Received bytes borrow until callback return. */\ncpkt_opcua_ConnectionManager *cpkt_opcua_ConnectionManager_fromPlugin(const cpkt_opcua_ConnectionManagerPlugin *plugin, void *context, cpkt_opcua_String name, cpkt_opcua_String protocol, const cpkt_opcua_KeyValueMap *params);',
        '/** Invoke native interrupt registration/removal. User context borrows until deregistration; a callback may deregister itself. Native dispatch handles and status are preserved. */\ncpkt_opcua_InterruptManager *cpkt_opcua_InterruptManager_fromPlugin(const cpkt_opcua_InterruptManagerPlugin *plugin, void *context, cpkt_opcua_String name, const cpkt_opcua_KeyValueMap *params);',
        '/** Borrow the custom source context (NULL on stock backends). */',
        'void *cpkt_opcua_EventSource_context(cpkt_opcua_EventSource *source);',
    ]
    # A custom backend provides the exact public method signatures. Metadata
    # lives on the opaque handle and is changed through its complete accessors.
    methods = public_body(path, 'EventLoop', config)
    declarations = []
    for result, name, signature in callbacks(methods):
        result = result.replace('UA_', 'cpkt_opcua_')
        signature = signature.replace('UA_', 'cpkt_opcua_')
        declarations.append(f'    {result} (*{name})({signature});')
    header += [
        '/** Complete custom event-loop method table derived from plugin/eventloop.h.',
        ' * The table is copied. context remains caller-owned until free returns Good.',
        ' * A successful free callback releases backend context, never the facade handle.',
        ' * Implement native lifecycle, scheduling, recursive locking and architecture',
        ' * integration. Every method is required. Metadata remains on the handle:',
        ' * use setState/setEventSources and source setInfo as the backend changes it.',
        ' * free must stop/deregister/free owned sources before returning Good.',
        ' * The facade performs ABI conversions; it adds no scheduler or transport.',
        ' * POSIX connection managers require the POSIX loop private architecture; this',
        ' * custom backend must use compatible sources, exactly as native plugins do. */',
        'typedef struct {', *declarations, '} cpkt_opcua_EventLoopPlugin;',
        '/** Inspect actual native loop state. No snapshot of scheduler/socket state is made; serialize with loop operations. */\ncpkt_opcua_EventLoop *cpkt_opcua_EventLoop_fromPlugin(const cpkt_opcua_EventLoopPlugin *plugin, void *context, const cpkt_opcua_log_config *logger, const cpkt_opcua_KeyValueMap *params);',
        '/** Borrow the custom backend context (NULL on stock loops). */',
        'void *cpkt_opcua_EventLoop_context(cpkt_opcua_EventLoop *loop);',
        '/** Custom backend metadata only. Stock loops reject mutation. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventLoop_setState(cpkt_opcua_EventLoop *loop, cpkt_opcua_EventLoopState state);',
        '/** Custom backend linked-list metadata. Source next/eventLoop/state must be',
        ' * set coherently through setInfo; register/deregister controls ownership.',
        ' * The facade does not replace a backend lifecycle or impose its own order. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventLoop_setEventSources(cpkt_opcua_EventLoop *loop, cpkt_opcua_EventSource *head);',
        '/** Set custom-backend source metadata. Name bytes are copied. State/list/loop',
        ' * are scalar/borrowed links; use within custom register/deregister/lifecycle',
        ' * callbacks. Stock loops own these fields and reject this operation. */',
        'cpkt_opcua_StatusCode cpkt_opcua_EventSource_setInfo(cpkt_opcua_EventSource *source, const cpkt_opcua_EventSourceInfo *info);',
    ]
    implementation = [
        '/* Generated complete native/custom event-loop method conversions. */',
        'typedef struct { UA_EventLoop native; cpkt_opcua_EventLoop *owner; } cpkt_loop_custom;',
    ]
    for result, name, signature in callbacks(methods):
        args = parameters(signature)
        if args[0] != ('UA_EventLoop *', 'el'):
            raise ValueError('Adapt custom loop receiver: ' + name)
        locals_, before, after, calls = [], [], [], ['loop']
        for spelling, arg in args[1:]:
            if spelling == 'UA_DateTime *':
                locals_.append(f'  cpkt_opcua_DateTime public_{arg};')
                before.append(f'  if({arg}) cpkt_loop_u64_public((UA_UInt64)*{arg}, &public_{arg});')
                after.append(f'  if({arg}) *{arg} = cpkt_loop_i64_native(&public_{arg});')
                calls.append(f'{arg} ? &public_{arg} : NULL')
            elif spelling == 'UA_UInt64 *':
                locals_.append(f'  cpkt_opcua_UInt64 public_{arg};')
                before.append(f'  memset(&public_{arg}, 0, sizeof(public_{arg}));')
                after.append(f'  if({arg} && !result) *{arg} = cpkt_loop_u64_native(&public_{arg});')
                calls.append(f'{arg} ? &public_{arg} : NULL')
            elif spelling == 'UA_UInt64':
                locals_.append(f'  cpkt_opcua_UInt64 public_{arg};')
                before.append(f'  cpkt_loop_u64_public({arg}, &public_{arg});')
                calls.append('public_' + arg)
            elif spelling == 'UA_TimerPolicy':
                calls.append(f'(cpkt_opcua_TimerPolicy){arg}')
            elif spelling == 'UA_DelayedCallback *':
                calls.append(f'(cpkt_opcua_DelayedCallback *){arg}')
            elif spelling == 'UA_EventSource *':
                locals_.append(f'  cpkt_opcua_EventSource *public_{arg};')
                before += [f'  public_{arg} = cpkt_loop_source_find(loop, {arg});',
                           f'  if(!public_{arg}) {{',
                           f'    public_{arg} = cpkt_loop_source_adopt(loop, {arg});',
                           f'    if(!public_{arg}) return UA_STATUSCODE_BADOUTOFMEMORY;', '  }']
                calls.append('public_' + arg)
            elif spelling in ('UA_Callback', 'void *', 'UA_UInt32', 'UA_Double'):
                calls.append(arg)
            else:
                raise ValueError('Adapt complete custom loop signature: ' + name + '.' + arg)
        implementation += [f'static {result} cpkt_custom_loop_{name}({signature}) {{',
                           '  cpkt_loop_custom *proxy = (cpkt_loop_custom *)el;',
                           '  cpkt_opcua_EventLoop *loop = proxy->owner;']
        if result != 'void':
            public_result = result.replace('UA_', 'cpkt_opcua_')
            implementation.append(f'  {public_result} result;')
        guarded = name not in ('lock', 'unlock', 'cancel')
        implementation += locals_ + before + (['  ++loop->depth;'] if guarded else [])
        call = f'loop->plugin.{name}({", ".join(calls)})'
        implementation += [f'  result = {call};' if result != 'void' else f'  {call};']
        implementation += (['  --loop->depth;'] if guarded else []) + after
        if name == 'free':
            implementation += ['  if(!result) { UA_KeyValueMap_clear(&el->params); UA_free(proxy); }']
        if result in ('UA_DateTime', 'UA_Int64'):
            implementation.append('  return cpkt_loop_i64_native(&result);')
        elif result != 'void':
            implementation.append('  return result;')
        implementation += ['}']
    implementation += [
        'cpkt_opcua_EventLoop *cpkt_opcua_EventLoop_fromPlugin(const cpkt_opcua_EventLoopPlugin *plugin, void *context, const cpkt_opcua_log_config *logger, const cpkt_opcua_KeyValueMap *params) {',
        '  cpkt_opcua_EventLoop *loop;', '  cpkt_loop_custom *proxy;',
        '  cpkt_opcua_log_config quiet;', '  UA_StatusCode status;',
        '  if(!plugin || !cpkt_logger_valid(logger)) return NULL;',
    ]
    for _, name, _ in callbacks(methods):
        implementation += [f'  if(!plugin->{name}) return NULL;']
    implementation += [
        '  loop = (cpkt_opcua_EventLoop *)UA_calloc(1, sizeof(*loop));',
        '  proxy = (cpkt_loop_custom *)UA_calloc(1, sizeof(*proxy));',
        '  if(!loop || !proxy) { UA_free(loop); UA_free(proxy); return NULL; }',
        '  status = cpkt_loop_map_to_native(params, &proxy->native.params);',
        '  if(status) { UA_KeyValueMap_clear(&proxy->native.params); UA_free(loop); UA_free(proxy); return NULL; }',
        '  loop->custom = 1; loop->context = context; loop->plugin = *plugin;',
        '  loop->native = &proxy->native; proxy->owner = loop;',
        '  memset(&quiet, 0, sizeof(quiet));',
        '  cpkt_logger_set(&loop->logger, &loop->logger.native, logger ? logger : &quiet);',
        '  proxy->native.logger = &loop->logger.native;',
    ]
    for _, name, _ in callbacks(methods):
        implementation += [f'  proxy->native.{name} = cpkt_custom_loop_{name};']
    implementation += ['  cpkt_loop_publish(loop);', '  return loop;', '}']
    (output / 'opcua_eventloop_metadata.inc').write_text('\n'.join(implementation) + '\n')
    return header
