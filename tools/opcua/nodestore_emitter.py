"""Expose the public nodestore methods over actual opaque native storage.

Node/tree layouts stay native. The method inventory and signatures come from
the upstream public header; this binding policy specifies only C89 boundaries.
"""
import re
from plugin_emitter import public_body, callbacks, parameters, validate_fields
from core_client_emitter import enum_declaration

METHODS = {
    'free': ('void', ['UA_Nodestore *']),
    'newNode': ('UA_Node *', ['UA_Nodestore *', 'UA_NodeClass']),
    'deleteNode': ('void', ['UA_Nodestore *', 'UA_Node *']),
    'getNode': ('const UA_Node *', ['UA_Nodestore *', 'const UA_NodeId *', 'UA_UInt32', 'UA_ReferenceTypeSet', 'UA_BrowseDirection']),
    'getNodeFromPtr': ('const UA_Node *', ['UA_Nodestore *', 'UA_NodePointer', 'UA_UInt32', 'UA_ReferenceTypeSet', 'UA_BrowseDirection']),
    'getEditNode': ('UA_Node *', ['UA_Nodestore *', 'const UA_NodeId *', 'UA_UInt32', 'UA_ReferenceTypeSet', 'UA_BrowseDirection']),
    'getEditNodeFromPtr': ('UA_Node *', ['UA_Nodestore *', 'UA_NodePointer', 'UA_UInt32', 'UA_ReferenceTypeSet', 'UA_BrowseDirection']),
    'releaseNode': ('void', ['UA_Nodestore *', 'const UA_Node *']),
    'getNodeCopy': ('UA_StatusCode', ['UA_Nodestore *', 'const UA_NodeId *', 'UA_Node **']),
    'insertNode': ('UA_StatusCode', ['UA_Nodestore *', 'UA_Node *', 'UA_NodeId *']),
    'replaceNode': ('UA_StatusCode', ['UA_Nodestore *', 'UA_Node *']),
    'removeNode': ('UA_StatusCode', ['UA_Nodestore *', 'const UA_NodeId *']),
    'getReferenceTypeId': ('const UA_NodeId *', ['UA_Nodestore *', 'UA_Byte']),
    'iterate': ('void', ['UA_Nodestore *', 'UA_NodestoreVisitor', 'void *']),
}


def c89_type(spelling, result=False):
    if result and spelling == 'const UA_NodeId *':
        return 'const cpkt_opcua_NodeIdStorage *'
    if spelling == 'UA_NodePointer':
        return 'const cpkt_opcua_NodePointer *'
    return spelling.replace('UA_', 'cpkt_opcua_')


def emit_nodestore(headers, output):
    text = (headers / 'plugin/nodestore.h').read_text()
    # The facade TU is C89, while this public native header has one C99 inline
    # loop. Stage the authoritative header with that syntax-only translation;
    # native layouts and algorithms are unchanged. This private file is not a
    # second C89 public representation of the native node/tree structures.
    old = '    UA_ReferenceTypeSet set;\n    for(size_t i = 0; i < UA_REFERENCETYPESET_MAX / 32; i++)'
    new = '    UA_ReferenceTypeSet set;\n    size_t i;\n    for(i = 0; i < UA_REFERENCETYPESET_MAX / 32; i++)'
    if text.count(old) != 1:
        raise ValueError('Adapt native nodestore C89 inline-header syntax')
    (output / 'opcua_native_nodestore.h').write_text(text.replace(old, new))
    body = public_body(headers / 'plugin/nodestore.h', 'Nodestore', (headers / 'config.h').read_text())
    validate_fields(body, [])
    slots = callbacks(body)
    if {name for _, name, _ in slots} != set(METHODS):
        raise ValueError('Adapt complete public Nodestore methods')
    maximum = re.search(r'#define UA_REFERENCETYPESET_MAX\s+(\d+)', text)
    if not maximum or int(maximum[1]) % 32:
        raise ValueError('Adapt complete ReferenceTypeSet width')
    h = ['/** Actual native node, tagged pointer, reference and identifier storage.',
         ' * No public backend layout is duplicated. Const handles borrow; never free',
         ' * or mutate them. Nodestore methods retain native ownership and identity:',
         ' * get/getEdit require releaseNode, copy/new require deleteNode or submission,',
         ' * insert/replace consume nodes EVEN ON NATIVE FAILURE. Serialize operations',
         ' * with the store/server. Runtime associations remain native and use move.',
         ' * Callback handles borrow until return; copy explicitly before retaining. */',
         'typedef struct cpkt_opcua_Node cpkt_opcua_Node;',
         '/** Opaque actual native NodePointer storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_NodePointer cpkt_opcua_NodePointer;',
         '/** Opaque actual native NodeIdStorage storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_NodeIdStorage cpkt_opcua_NodeIdStorage;',
         '/** Opaque actual native ExpandedNodeIdStorage storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_ExpandedNodeIdStorage cpkt_opcua_ExpandedNodeIdStorage;',
         '/** Opaque actual native NodeReferenceKind storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_NodeReferenceKind cpkt_opcua_NodeReferenceKind;',
         '/** Opaque actual native ReferenceTarget storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_ReferenceTarget cpkt_opcua_ReferenceTarget;',
         '/** Opaque actual native LocalizedTextListEntry storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_LocalizedTextListEntry cpkt_opcua_LocalizedTextListEntry;',
         '/** Opaque actual native Nodestore storage; use its public accessors and matching ownership operations. */\ntypedef struct cpkt_opcua_Nodestore cpkt_opcua_Nodestore;',
         f'#define CPKT_OPCUA_REFERENCETYPESET_MAX {maximum[1]}',
         '/** Native fixed 128-bit reference-type bitset in C89 words; it owns no dynamic storage. */\ntypedef struct { cpkt_opcua_UInt32 bits[CPKT_OPCUA_REFERENCETYPESET_MAX / 32]; } cpkt_opcua_ReferenceTypeSet;',
         'extern const cpkt_opcua_ReferenceTypeSet cpkt_opcua_REFERENCETYPESET_NONE;',
         'extern const cpkt_opcua_ReferenceTypeSet cpkt_opcua_REFERENCETYPESET_ALL;']
    for name, value in re.findall(r'#define UA_(REFERENCETYPEINDEX_\w+)\s+(\d+)', text):
        h += [f'#define CPKT_OPCUA_{name} {value}']
    h += ['/** Native fixed 128-bit reference type set operation. Indices use native bounds and no owned storage; inputs borrow during the call. */\nvoid cpkt_opcua_ReferenceTypeSet_init(cpkt_opcua_ReferenceTypeSet *set);',
          '/** Native fixed 128-bit reference type set operation. Indices use native bounds and no owned storage; inputs borrow during the call. */\ncpkt_opcua_ReferenceTypeSet cpkt_opcua_REFTYPESET(cpkt_opcua_Byte index);',
          '/** Native fixed 128-bit reference type set operation. Indices use native bounds and no owned storage; inputs borrow during the call. */\nvoid cpkt_opcua_ReferenceTypeSet_add(cpkt_opcua_ReferenceTypeSet *set, cpkt_opcua_Byte index);',
          '/** Native fixed 128-bit reference type set operation. Indices use native bounds and no owned storage; inputs borrow during the call. */\ncpkt_opcua_ReferenceTypeSet cpkt_opcua_ReferenceTypeSet_union(cpkt_opcua_ReferenceTypeSet a, cpkt_opcua_ReferenceTypeSet b);',
          '/** Native fixed 128-bit reference type set operation. Indices use native bounds and no owned storage; inputs borrow during the call. */\ncpkt_opcua_Boolean cpkt_opcua_ReferenceTypeSet_contains(const cpkt_opcua_ReferenceTypeSet *set, cpkt_opcua_Byte index);',
          '/** Native nodestore visitor. The node borrows only through this callback; do not mutate traversal state or destroy the backend. */\ntypedef void (*cpkt_opcua_NodestoreVisitor)(void *context, const cpkt_opcua_Node *node);',
          '/** Complete native plugin callback table. The table is copied; context',
          ' * cleanup transfers on successful construction. free releases backend',
          ' * data only; facade backing is released afterward. No store may be freed',
          ' * from its active methods/visitor. getReferenceTypeId returns actual native',
          ' * identifier backing (owned or node-borrowed), stable until store deletion. */',
          'typedef struct {']
    impl, assign = [], []
    for returns, name, signature in slots:
        args = parameters(signature)
        if (returns, [t for t, _ in args]) != METHODS[name]:
            raise ValueError('Adapt Nodestore method signature: ' + name)
        public = ', '.join(c89_type(t) + ' ' + n for t, n in args)
        ret = c89_type(returns, True)
        h += [f'  {ret} (*{name})({public});']
        if name == 'free':
            continue
        # Native -> custom C89 backend. Node and pointer storage are passed
        # directly; only public schema values cross a representation boundary.
        decl, setup, finish, cleanup, call = [], [], [], [], ['store']
        for spelling, param in args[1:]:
            if spelling in ('const UA_Node *', 'UA_Node *'):
                call += [f'({c89_type(spelling)})(void *){param}']
            elif spelling == 'UA_Node **':
                decl += ['  cpkt_opcua_Node *value_node = NULL;']
                call += [f'{param} ? &value_node : NULL']
                finish += [f'  if({param}) *{param} = (UA_Node *)(void *)value_node;']
            elif spelling == 'const UA_NodeId *':
                decl += [f'  cpkt_opcua_NodeId value_{param};']
                setup += [f'  memset(&value_{param}, 0, sizeof(value_{param}));',
                          f'  if({param}) cpkt_hb_borrow_node({param}, &value_{param});']
                call += [f'{param} ? &value_{param} : NULL']
            elif spelling == 'UA_NodeId *':
                decl += [f'  cpkt_opcua_NodeId value_{param};', f'  UA_NodeId staged_{param};']
                setup += [f'  memset(&value_{param}, 0, sizeof(value_{param}));', f'  UA_NodeId_init(&staged_{param});']
                call += [f'{param} ? &value_{param} : NULL']
                finish += [f'  if({param}) {{', f'    status = cpkt_convert(&value_{param}, &staged_{param}, &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);',
                           f'    if(!status) {{ *{param} = staged_{param}; UA_NodeId_init(&staged_{param}); }}', '  }']
                cleanup += [f'  cpkt_opcua_NodeId_clear(&value_{param});', f'  UA_NodeId_clear(&staged_{param});']
            elif spelling == 'UA_NodePointer':
                call += [f'(const cpkt_opcua_NodePointer *)(const void *)&{param}']
            elif spelling == 'UA_ReferenceTypeSet':
                decl += [f'  cpkt_opcua_ReferenceTypeSet value_{param};']
                setup += [f'  value_{param} = cpkt_store_references_read({param});']
                call += [f'value_{param}']
            elif spelling == 'UA_NodestoreVisitor':
                decl += ['  cpkt_store_visit_native visit;']
                setup += ['  visit.callback = visitor; visit.context = visitorCtx;']
                call += ['visitor ? cpkt_store_visit_to_native : NULL']
            elif name == 'iterate' and param == 'visitorCtx':
                call += ['&visit']
            else:
                call += [f'({c89_type(spelling)}){param}']
        is_status = returns == 'UA_StatusCode'
        result = '  ' + ret + ' result;' if returns != 'void' else None
        impl += [f'static {returns} cpkt_store_custom_{name}({signature}) {{',
                 '  cpkt_opcua_Nodestore *store = cpkt_store_find(ns);',
                 *([result] if result else []), *(['  UA_StatusCode status = 0;'] if finish else []),
                 *decl, *setup, '  cpkt_store_enter(store);',
                 f'  {"result = " if result else ""}store->plugin.{name}({", ".join(call)});',
                 *finish, *cleanup, '  cpkt_store_leave(store);']
        if returns != 'void':
            if is_status:
                impl += ['  return status ? status : result;' if finish else '  return result;']
            elif '*' in returns:
                impl += [f'  return ({returns})(void *)result;']
            else:
                impl += [f'  return ({returns})result;']
        impl += ['}']
        assign += [f'  native->{name} = cpkt_store_custom_{name};']
    h += ['} cpkt_opcua_NodestorePlugin;',
          '/** Allocate a caller-owned native ZipTree backend; NULL reports failure. Installation transfers ownership to configuration; free a detached backend with Nodestore_free. */\ncpkt_opcua_Nodestore *cpkt_opcua_Nodestore_ZipTree(void);',
          '/** Allocate native dispatch backing for the full C89 backend table. The table is copied; context remains caller-owned through free. NULL reports validation/allocation failure. */\ncpkt_opcua_Nodestore *cpkt_opcua_Nodestore_fromPlugin(const cpkt_opcua_NodestorePlugin *plugin, void *context);',
          '/** Borrow the original backend context; no snapshot or ownership transfer occurs. */\nvoid *cpkt_opcua_Nodestore_getContext(const cpkt_opcua_Nodestore *store);',
          '/** Free a detached/quiescent store. A configured store is owned by its',
          ' * configuration and cannot be freed independently. NULL is safe. */',
          'cpkt_opcua_StatusCode cpkt_opcua_Nodestore_free(cpkt_opcua_Nodestore *store);']
    for returns, name, signature in slots:
        if name == 'free': continue
        method_contract = {'newNode': 'Allocate a detached node through this backend for its class. Insert/replace consumes it by native rules; otherwise release through this backend deleteNode.', 'deleteNode': 'Release detached node storage through its allocating backend. Never delete a borrowed node or plain copy_alloc storage through an incompatible backend.', 'getNode': 'Borrow the actual backend node with native attribute/reference selection. Release each successful borrow through releaseNode; mutation is restricted to editable nodes.', 'getNodeFromPtr': 'Borrow the actual backend node with native attribute/reference selection. Release each successful borrow through releaseNode; mutation is restricted to editable nodes.', 'getEditNode': 'Borrow the actual backend node with native attribute/reference selection. Release each successful borrow through releaseNode; mutation is restricted to editable nodes.', 'getEditNodeFromPtr': 'Borrow the actual backend node with native attribute/reference selection. Release each successful borrow through releaseNode; mutation is restricted to editable nodes.', 'releaseNode': 'Release exactly one backend getNode/getEditNode borrow. The backend owns the node; no facade snapshot is introduced.', 'getNodeCopy': 'Return an owned detached native copy via this backend. Output starts NULL; release through backend deleteNode or transfer with replaceNode.', 'insertNode': 'Transfer detached node ownership to the native backend, including native failure consumption. Preserve its identifier/concurrency status; do not access the node after submission.', 'replaceNode': 'Transfer detached node ownership to the native backend, including native failure consumption. Preserve its identifier/concurrency status; do not access the node after submission.', 'removeNode': 'Invoke the native nodestore method with original backend context. Serialize with configuration/backend changes; ownership follows the native method contract.', 'getReferenceTypeId': 'Borrow the backend reference type identity for this index; keep the backend alive and serialize mutations.', 'iterate': 'Invoke the native backend visitor. Nodes borrow only through each callback; do not destroy the backend or mutate iteration state during dispatch.'}
        h += ['/** ' + method_contract[name] + ' */',
              c89_type(returns, True) + ' cpkt_opcua_Nodestore_' + name + '(' + ', '.join(c89_type(t) + ' ' + n for t, n in parameters(signature)) + ');']
    impl += ['static void cpkt_store_assign(UA_Nodestore *native) {', *assign, '  native->free = cpkt_store_custom_free;', '}']
    h += storage_declarations(headers)
    node_header, node_impl = emit_node_values()
    h += node_header
    impl += node_impl
    (output / 'opcua_nodestore_metadata.inc').write_text('\n'.join(impl) + '\n')
    return h


def emit_node_values():
    h = ['/** Read stored public attributes into an empty owned C89 record of the',
         ' * matching class. Includes the first display-name/description locale;',
         ' * use list accessors for all locales. Dynamic user access/executability',
         ' * attributes are not stored. Callback value sources are not invoked;',
         ' * their VALUE bit is absent. Other source/runtime state remains native. */',
         'const cpkt_opcua_Type *cpkt_opcua_Node_getAttributesType(const cpkt_opcua_Node *node);',
         '/** Inspect actual native node metadata using its public C89 representation. Serialize with node mutation; owned schema outputs start empty and require their matching clear. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_getStoredAttributes(const cpkt_opcua_Node *node, void *attributes, const cpkt_opcua_Type *type);',
         '/** Inspect actual native node metadata using its public C89 representation. Serialize with node mutation; owned schema outputs start empty and require their matching clear. */\ncpkt_opcua_ValueSourceType cpkt_opcua_Node_getValueSourceType(const cpkt_opcua_Node *node);',
         '/** Borrow the ACTUAL internal DataValue. Mutable access requires an editable',
         ' * variable/type node. NULL for external/callback sources or other classes.',
         ' * Never free borrowed history_value backing. Native copy retains callback',
         ' * and external-slot identities; facade server source setters configure them. */',
         'const cpkt_opcua_history_value *cpkt_opcua_Node_getInternalValue(const cpkt_opcua_Node *node);',
         '/** Inspect actual native node metadata using its public C89 representation. Serialize with node mutation; owned schema outputs start empty and require their matching clear. */\ncpkt_opcua_history_value *cpkt_opcua_Node_getEditInternalValue(cpkt_opcua_Node *node);',
         '/** Inspect actual native node metadata using its public C89 representation. Serialize with node mutation; owned schema outputs start empty and require their matching clear. */\ncpkt_opcua_Boolean cpkt_opcua_Node_getIsDynamic(const cpkt_opcua_Node *node);',
         '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_setIsDynamic(cpkt_opcua_Node *node, cpkt_opcua_Boolean value);',
         '/** Borrow actual native node storage. The view invalidates on mutation, release or deletion; edit only detached/editable nodes. Never clear borrowed storage. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_getReferenceTypeState(const cpkt_opcua_Node *node, cpkt_opcua_Byte *index, cpkt_opcua_ReferenceTypeSet *subtypes);',
         '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_setReferenceTypeState(cpkt_opcua_Node *node, cpkt_opcua_Byte index, cpkt_opcua_ReferenceTypeSet subtypes);']
    classes = {'Object': ['eventNotifier'], 'Variable': ['accessLevel', 'minimumSamplingInterval', 'historizing'],
               'Method': ['executable'], 'ObjectType': ['isAbstract'], 'VariableType': ['isAbstract'],
               'ReferenceType': ['isAbstract', 'symmetric', 'inverseName'], 'DataType': ['isAbstract'],
               'View': ['containsNoLoops', 'eventNotifier']}
    impl = ['const cpkt_opcua_Type *cpkt_opcua_Node_getAttributesType(const cpkt_opcua_Node *node) {',
            '  const UA_Node *native = (const UA_Node *)(const void *)node;',
            '  if(!node) return NULL;', '  switch(native->head.nodeClass) {']
    for cls in classes:
        impl += [f'  case UA_NODECLASS_{cls.upper()}: return &cpkt_types[CPKT_OPCUA_TYPES_{cls.upper()}ATTRIBUTES];']
    impl += ['  default: return NULL;', '  }', '}',
             'cpkt_opcua_StatusCode cpkt_opcua_Node_getStoredAttributes(const cpkt_opcua_Node *node, void *attributes, const cpkt_opcua_Type *type) {',
             '  const UA_Node *native = (const UA_Node *)(const void *)node;',
             '  if(!node || !attributes || !type) return UA_STATUSCODE_BADINVALIDARGUMENT;',
             '  if(type != cpkt_opcua_Node_getAttributesType(node)) return UA_STATUSCODE_BADNODEATTRIBUTESINVALID;',
             '  switch(native->head.nodeClass) {']
    for cls, fields in classes.items():
        attr = cls + 'Attributes'
        field = cls[0].lower() + cls[1:] + 'Node'
        impl += [f'  case UA_NODECLASS_{cls.upper()}: {{', f'    UA_{attr} value;', '    memset(&value, 0, sizeof(value));',
                 '    value.writeMask = native->head.writeMask;',
                 '    value.specifiedAttributes = UA_NODEATTRIBUTESMASK_WRITEMASK;',
                 '    if(native->head.displayName) { value.displayName = native->head.displayName->localizedText; value.specifiedAttributes |= UA_NODEATTRIBUTESMASK_DISPLAYNAME; }',
                 '    if(native->head.description) { value.description = native->head.description->localizedText; value.specifiedAttributes |= UA_NODEATTRIBUTESMASK_DESCRIPTION; }']
        for f in fields:
            impl += [f'    value.{f} = native->{field}.{f};', f'    value.specifiedAttributes |= UA_NODEATTRIBUTESMASK_{f.upper()};']
        if cls in ('Variable', 'VariableType'):
            impl += [f'    value.dataType = native->{field}.dataType;', f'    value.valueRank = native->{field}.valueRank;',
                     f'    value.arrayDimensions = native->{field}.arrayDimensions;', f'    value.arrayDimensionsSize = native->{field}.arrayDimensionsSize;',
                     '    value.specifiedAttributes |= UA_NODEATTRIBUTESMASK_DATATYPE | UA_NODEATTRIBUTESMASK_VALUERANK | UA_NODEATTRIBUTESMASK_ARRAYDIMENSIONS;',
                     f'    if(native->{field}.valueSourceType == UA_VALUESOURCETYPE_INTERNAL) {{', f'      value.value = native->{field}.valueSource.internal.value.value;',
                     '      value.specifiedAttributes |= UA_NODEATTRIBUTESMASK_VALUE;',
                     f'    }} else if(native->{field}.valueSourceType == UA_VALUESOURCETYPE_EXTERNAL && native->{field}.valueSource.external.value && *native->{field}.valueSource.external.value) {{',
                     f'      value.value = (*native->{field}.valueSource.external.value)->value;', '      value.specifiedAttributes |= UA_NODEATTRIBUTESMASK_VALUE;', '    }']
        impl += ['    return cpkt_convert(&value, attributes, type, 0, 0);', '  }']
    impl += ['  default: return UA_STATUSCODE_BADNODEATTRIBUTESINVALID;', '  }', '}']
    return h, impl


def storage_declarations(headers):
    h = []
    for name, filename in [('ValueSourceType', 'server.h')]:
        declaration, _ = enum_declaration((headers / filename).read_text(), name)
        h += ['/** Native variable value-source discriminant. Only the selected backend arm is active. */', declaration]
    h += ['/** Identifier storage is the actual native object. new/delete own it;',
          ' * node/store getters return const borrowed storage. get returns an owned',
          ' * C89 schema copy into empty output. set converts first then replaces',
          ' * owned contents; do not mutate/free borrowed storage. Serialize access. */']
    for name in ('NodeId', 'ExpandedNodeId'):
        h += ['/** Allocate an owned empty native identity backing; NULL reports allocation failure. Release after every borrowing tagged pointer has ended. */\n' f'cpkt_opcua_{name}Storage *cpkt_opcua_{name}Storage_new(void);',
              '/** Clear and release owned native identity backing after every borrowing pointer has ended. */\n' f'void cpkt_opcua_{name}Storage_delete(cpkt_opcua_{name}Storage *storage);',
              '/** Copy actual native identity into an empty owned C89 output; clear the output with its schema clear after use. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}Storage_get(const cpkt_opcua_{name}Storage *storage, cpkt_opcua_{name} *value);',
              '/** Copy the C89 identity into native owned backing. Input borrows through return; failure preserves the previous identity. Serialize with borrowed tagged pointers. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}Storage_set(cpkt_opcua_{name}Storage *storage, const cpkt_opcua_{name} *value);']
    h += ['/** Native tagged-pointer backing. Factories do NOT copy identifier storage:',
          ' * fromNodeId/fromExpandedNodeId borrow its actual native address, exactly',
          ' * as upstream; retain it unchanged through all pointer users. copy creates',
          ' * native-owned targets. clear frees owned targets and MUST NOT be used on',
          ' * borrowed factory/visitor results. deleteBacking frees ONLY the allocated',
          ' * slot; clear owned targets first. Native node targets remain node-owned.',
          ' * init/from/copy require an empty destination. Callback pointers borrow',
          ' * stack/native storage until return and must not be deleted or retained. */',
          'cpkt_opcua_NodePointer *cpkt_opcua_NodePointer_new(void);',
          '/** Operate on actual native tagged pointer backing. Target ownership follows fromNodeId/fromExpandedNodeId/copy; preserve borrowed target lifetimes and use the matching clear/backing release. */\nvoid cpkt_opcua_NodePointer_deleteBacking(cpkt_opcua_NodePointer *pointer);',
          '/** Operate on actual native tagged pointer backing. Target ownership follows fromNodeId/fromExpandedNodeId/copy; preserve borrowed target lifetimes and use the matching clear/backing release. */\nvoid cpkt_opcua_NodePointer_init(cpkt_opcua_NodePointer *pointer);',
          '/** Clear owned nested fields and reset the record. Do not clear borrowed or arena-backed results; backing record storage remains caller-owned. */\nvoid cpkt_opcua_NodePointer_clear(cpkt_opcua_NodePointer *pointer);',
          '/** Copy into an empty destination with independent owned schema fields. Contexts and native callback/storage references retain their borrowing rules; clear the owned result once. */\ncpkt_opcua_StatusCode cpkt_opcua_NodePointer_copy(const cpkt_opcua_NodePointer *source, cpkt_opcua_NodePointer *destination);',
          '/** Inspect/compare actual native tagged pointers without copying targets. Borrowed target storage must remain alive. */\ncpkt_opcua_Boolean cpkt_opcua_NodePointer_isLocal(const cpkt_opcua_NodePointer *pointer);',
          '/** Inspect/compare actual native tagged pointers without copying targets. Borrowed target storage must remain alive. */\ncpkt_opcua_Order cpkt_opcua_NodePointer_order(const cpkt_opcua_NodePointer *a, const cpkt_opcua_NodePointer *b);',
          '/** Inspect/compare actual native tagged pointers without copying targets. Borrowed target storage must remain alive. */\ncpkt_opcua_Boolean cpkt_opcua_NodePointer_equal(const cpkt_opcua_NodePointer *a, const cpkt_opcua_NodePointer *b);',
          '/** Operate on actual native tagged pointer backing. Target ownership follows fromNodeId/fromExpandedNodeId/copy; preserve borrowed target lifetimes and use the matching clear/backing release. */\nvoid cpkt_opcua_NodePointer_fromNodeId(const cpkt_opcua_NodeIdStorage *id, cpkt_opcua_NodePointer *pointer);',
          '/** Operate on actual native tagged pointer backing. Target ownership follows fromNodeId/fromExpandedNodeId/copy; preserve borrowed target lifetimes and use the matching clear/backing release. */\nvoid cpkt_opcua_NodePointer_fromExpandedNodeId(const cpkt_opcua_ExpandedNodeIdStorage *id, cpkt_opcua_NodePointer *pointer);',
          '/** Native conversion followed by an owned C89 schema copy. Empty output',
          ' * required. toNodeId retains upstream discarding of remote URI/index. */',
          'cpkt_opcua_StatusCode cpkt_opcua_NodePointer_toNodeId(const cpkt_opcua_NodePointer *pointer, cpkt_opcua_NodeId *id);',
          '/** Operate on actual native tagged pointer backing. Target ownership follows fromNodeId/fromExpandedNodeId/copy; preserve borrowed target lifetimes and use the matching clear/backing release. */\ncpkt_opcua_StatusCode cpkt_opcua_NodePointer_toExpandedNodeId(const cpkt_opcua_NodePointer *pointer, cpkt_opcua_ExpandedNodeId *id);',
          '/** Public head metadata. nodeId/browseName are owned C89 copies; clear once.',
          ' * Reference/localization getters below borrow actual native storage and',
          ' * invalidate on mutation/release/deletion. Private runtime associations',
          ' * are preserved by native copy/move; they are never represented as C89 types. */',
          'typedef struct { cpkt_opcua_NodeId nodeId; cpkt_opcua_NodeClass nodeClass; cpkt_opcua_QualifiedName browseName; cpkt_opcua_UInt32 writeMask; size_t referencesSize; void *context; cpkt_opcua_Boolean constructed; } cpkt_opcua_NodeInfo;',
          '/** Inspect actual native node metadata using its public C89 representation. Serialize with node mutation; owned schema outputs start empty and require their matching clear. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_getInfo(const cpkt_opcua_Node *node, cpkt_opcua_NodeInfo *info);',
          '/** Clear owned nested fields and reset the record. Do not clear borrowed or arena-backed results; backing record storage remains caller-owned. */\nvoid cpkt_opcua_NodeInfo_clear(cpkt_opcua_NodeInfo *info);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_setNodeId(cpkt_opcua_Node *node, const cpkt_opcua_NodeId *id);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_setBrowseName(cpkt_opcua_Node *node, const cpkt_opcua_QualifiedName *name);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\nvoid cpkt_opcua_Node_setContext(cpkt_opcua_Node *node, void *context);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\nvoid cpkt_opcua_Node_setConstructed(cpkt_opcua_Node *node, cpkt_opcua_Boolean constructed);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\nvoid cpkt_opcua_Node_setWriteMask(cpkt_opcua_Node *node, cpkt_opcua_UInt32 mask);',
          '/** Borrow actual native node storage. The view invalidates on mutation, release or deletion; edit only detached/editable nodes. Never clear borrowed storage. */\nconst cpkt_opcua_NodeIdStorage *cpkt_opcua_Node_getNodeIdStorage(const cpkt_opcua_Node *node);',
          '/** Borrow actual native node storage. The view invalidates on mutation, release or deletion; edit only detached/editable nodes. Never clear borrowed storage. */\nconst cpkt_opcua_NodeReferenceKind *cpkt_opcua_Node_getReference(const cpkt_opcua_Node *node, size_t index);',
          '/** Borrow actual native node storage. The view invalidates on mutation, release or deletion; edit only detached/editable nodes. Never clear borrowed storage. */\ncpkt_opcua_NodeReferenceKind *cpkt_opcua_Node_getEditReference(cpkt_opcua_Node *node, size_t index);',
          '/** Borrow actual native node storage. The view invalidates on mutation, release or deletion; edit only detached/editable nodes. Never clear borrowed storage. */\nconst cpkt_opcua_LocalizedTextListEntry *cpkt_opcua_Node_getDisplayName(const cpkt_opcua_Node *node);',
          '/** Borrow actual native node storage. The view invalidates on mutation, release or deletion; edit only detached/editable nodes. Never clear borrowed storage. */\nconst cpkt_opcua_LocalizedTextListEntry *cpkt_opcua_Node_getDescription(const cpkt_opcua_Node *node);',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\nconst cpkt_opcua_LocalizedTextListEntry *cpkt_opcua_LocalizedTextListEntry_next(const cpkt_opcua_LocalizedTextListEntry *entry);',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\ncpkt_opcua_StatusCode cpkt_opcua_LocalizedTextListEntry_get(const cpkt_opcua_LocalizedTextListEntry *entry, cpkt_opcua_LocalizedText *value);',
          '/** Delegate actual native node helpers. Attributes use their matching',
          ' * public generated type. Only detached/editable nodes may be mutated;',
          ' * nodeClass is fixed by its allocator. Attribute inputs borrow until return.',
          ' * setAttributes requires a fresh node: native copies attribute fields without',
          ' * clearing their previous contents. It is not an update of a populated node.',
          ' * copy resets an allocated destination of the same class; self-copy is safe.',
          ' * copy_alloc creates plain native storage; release via delete_alloc, NEVER',
          ' * via a backend-specific deleteNode unless that backend accepts it. */',
          'cpkt_opcua_StatusCode cpkt_opcua_Node_setAttributes(cpkt_opcua_Node *node, const void *attributes, const cpkt_opcua_Type *attributeType);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_insertOrUpdateDisplayName(cpkt_opcua_Node *node, const cpkt_opcua_LocalizedText *value);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_insertOrUpdateDescription(cpkt_opcua_Node *node, const cpkt_opcua_LocalizedText *value);',
          '/** Copy into an empty destination with independent owned schema fields. Contexts and native callback/storage references retain their borrowing rules; clear the owned result once. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_copy(const cpkt_opcua_Node *source, cpkt_opcua_Node *destination);',
          '/** Move native monitored-item associations between compatible nodes. No facade queue or copies are added; both nodes must be editable and remain alive through the call. */\nvoid cpkt_opcua_Node_moveMonitoredItems(cpkt_opcua_Node *source, cpkt_opcua_Node *destination);',
          '/** Allocate a plain native node copy. Release with Node_delete_alloc, not a backend allocator unless it explicitly accepts this storage. */\ncpkt_opcua_Node *cpkt_opcua_Node_copy_alloc(const cpkt_opcua_Node *source);',
          '/** Release plain native node storage created by Node_copy_alloc. Never use for backend-specific allocations or borrowed nodes. */\nvoid cpkt_opcua_Node_delete_alloc(cpkt_opcua_Node *node);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_addReference(cpkt_opcua_Node *node, cpkt_opcua_Byte type, cpkt_opcua_Boolean forward, const cpkt_opcua_ExpandedNodeId *target, cpkt_opcua_UInt32 nameHash);',
          '/** Mutate actual native node fields through the public C89 boundary. Use detached/editable nodes and serialize access; schema inputs borrow through return, callback/context references keep native lifetimes. */\ncpkt_opcua_StatusCode cpkt_opcua_Node_deleteReference(cpkt_opcua_Node *node, cpkt_opcua_Byte type, cpkt_opcua_Boolean forward, const cpkt_opcua_ExpandedNodeId *target);',
          '/** Remove native reference storage (or all except the supplied keep set). Mutate only detached/editable nodes and invalidate prior reference views. */\nvoid cpkt_opcua_Node_deleteReferencesSubset(cpkt_opcua_Node *node, const cpkt_opcua_ReferenceTypeSet *keep);',
          '/** Remove native reference storage (or all except the supplied keep set). Mutate only detached/editable nodes and invalidate prior reference views. */\nvoid cpkt_opcua_Node_deleteReferences(cpkt_opcua_Node *node);',
          '/** Clear owned nested fields and reset the record. Do not clear borrowed or arena-backed results; backing record storage remains caller-owned. */\nvoid cpkt_opcua_Node_clear(cpkt_opcua_Node *node);',
          '/** Public native metadata snapshot. Scalar fields are copied; handle and storage pointers borrow their owning native object. */\ntypedef struct { size_t targetsSize; cpkt_opcua_Boolean hasRefTree; cpkt_opcua_Byte referenceTypeIndex; cpkt_opcua_Boolean isInverse; } cpkt_opcua_NodeReferenceKindInfo;',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\ncpkt_opcua_StatusCode cpkt_opcua_NodeReferenceKind_getInfo(const cpkt_opcua_NodeReferenceKind *kind, cpkt_opcua_NodeReferenceKindInfo *info);',
          '/** Native target visitor. Targets borrow during the call; a non-NULL return stops iteration. Do not mutate references during traversal. */\ntypedef void *(*cpkt_opcua_NodeReferenceKind_iterateCallback)(void *context, cpkt_opcua_ReferenceTarget *target);',
          '/** Iterate actual targets in native order; first non-NULL return stops.',
          ' * Targets borrow through callback; do not modify references during iteration. */',
          'void *cpkt_opcua_NodeReferenceKind_iterate(cpkt_opcua_NodeReferenceKind *kind, cpkt_opcua_NodeReferenceKind_iterateCallback callback, void *context);',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\nconst cpkt_opcua_ReferenceTarget *cpkt_opcua_NodeReferenceKind_findTarget(const cpkt_opcua_NodeReferenceKind *kind, const cpkt_opcua_ExpandedNodeId *target);',
          '/** Switch the native reference target representation, preserving target identities. Existing target views invalidate; mutate only editable nodes. */\ncpkt_opcua_StatusCode cpkt_opcua_NodeReferenceKind_switch(cpkt_opcua_NodeReferenceKind *kind);',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\nconst cpkt_opcua_NodePointer *cpkt_opcua_ReferenceTarget_getId(const cpkt_opcua_ReferenceTarget *target);',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\ncpkt_opcua_UInt32 cpkt_opcua_ReferenceTarget_getNameHash(const cpkt_opcua_ReferenceTarget *target);',
          '/** Access actual native node reference/localization storage. Pointers borrow the owning node and invalidate on mutation/release; owned C89 schema outputs start empty and require their matching clear. */\nvoid cpkt_opcua_ReferenceTarget_setNameHash(cpkt_opcua_ReferenceTarget *target, cpkt_opcua_UInt32 hash);']
    return h
