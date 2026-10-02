#!/usr/bin/env python3
"""Observable failures of the public C89 coverage contract."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import sys
import subprocess
import tempfile

sys.dont_write_bytecode = True
repo = Path(sys.argv[1])
spec = importlib.util.spec_from_file_location('api', repo / 'tools/opcua/public_api.py')
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)

source = '''
struct UA_Plugin;
typedef struct UA_Plugin UA_Plugin;
struct UA_Plugin {
    void *context;
    unsigned int first, second;
    void (*callback)(const UA_Plugin *plugin, void *context);
    union { unsigned int number; void *data; } payload;
    unsigned char bits : 1;
    unsigned int dimensions[2];
};
typedef void (*UA_ResultCallback)(UA_Plugin *plugin);
typedef enum { MATCH_EQUAL, MATCH_AFTER } MatchStrategy;
extern const unsigned int UA_CATALOGUE[8];
__attribute__((visibility("default"))) unsigned int
UA_operation(const UA_Plugin *plugin);
static inline void UA_implementation(UA_Plugin *plugin) {
    if(plugin) { UA_operation(plugin); }
}
typedef struct { unsigned int a : 1; } static_assertion_failed_42;
'''
model = api.extract(source)
assert set(model['functions']) == {'UA_operation', 'UA_implementation'}
assert set(model['types']) == {'UA_Plugin', 'UA_ResultCallback', 'MatchStrategy'}
assert model['variables'] == {'UA_CATALOGUE': 'const unsigned int UA_CATALOGUE [ 8 ]'}
assert api.record_fields(model['types']['UA_Plugin']) == {
    'context', 'first', 'second', 'callback', 'payload', 'bits', 'dimensions'}
# A typedef placed after the complete tag must not hide the callback slots.
reordered = api.extract('struct UA_X { void (*hook)(void); }; typedef struct UA_X UA_X;')
assert api.record_fields(reordered['types']['UA_X']) == {'hook'}

contract = {category: {name: {'declaration': declaration, 'state': 'pending'}
                       for name, declaration in declarations.items()}
            for category, declarations in model.items()}
contract['functions']['UA_operation'].update(state='bound', binding='cpkt_opcua_operation')
contract['functions']['UA_implementation'].update(state='implementation', reason='Internal helper')
contract['types']['UA_Plugin'].update(state='bound', binding='cpkt_opcua_Plugin')
public = {'cpkt_opcua_operation', 'cpkt_opcua_Plugin'}
public_types = {'cpkt_opcua_Plugin': model['types']['UA_Plugin']}
result = api.check(model, contract, public, public_types=public_types)
assert set(result['bound']) == {'UA_operation', 'UA_Plugin'}
assert result['implementation'] == ['UA_implementation']
assert 'MatchStrategy' in result['pending']


def rejected(model=model, contract=contract, public=public,
             public_types=public_types, platform='linux', message=''):
    try:
        api.check(model, contract, public, platform=platform, public_types=public_types)
    except ValueError as error:
        assert message in str(error), str(error)
    else:
        raise AssertionError('Contract accepted invalid coverage: ' + message)


changed = deepcopy(model)
changed['functions']['UA_new_api'] = 'void UA_new_api ( void )'
rejected(model=changed, message='Unclassified public functions: UA_new_api')
changed = deepcopy(model)
changed['functions']['UA_operation'] += ' changed'
rejected(model=changed, message='Changed public functions declaration: UA_operation')
changed = deepcopy(model)
changed['types']['UA_Plugin'] = changed['types']['UA_Plugin'].replace('bits', 'new_field')
rejected(model=changed, message='Changed public types declaration: UA_Plugin')
rejected(public={'cpkt_opcua_Plugin'}, message='Missing C89 binding')
rejected(public_types={'cpkt_opcua_Plugin': 'typedef struct cpkt_opcua_Plugin cpkt_opcua_Plugin'},
         message='Missing C89 record members')
# Opaque native storage is covered by explicit public fields/accessors. An
# unexplained exclusion, typo, newly missing field or native escape must fail.
opaque_types = {'cpkt_opcua_Plugin': 'typedef struct cpkt_opcua_Plugin cpkt_opcua_Plugin',
                'cpkt_opcua_PluginInfo': model['types']['UA_Plugin']}
represented = deepcopy(contract)
represented['types']['UA_Plugin'].update(
    representation='Actual native storage; owned public metadata snapshot',
    fields={field: ['cpkt_opcua_PluginInfo.' + field] for field in api.record_fields(model['types']['UA_Plugin'])})
opaque_public = public | {'cpkt_opcua_PluginInfo'}
api.check(model, represented, opaque_public, public_types=opaque_types)
invalid = deepcopy(represented)
invalid['types']['UA_Plugin']['fields'] = ['first']
rejected(contract=invalid, public=opaque_public, public_types=opaque_types, message='Invalid public field map')
invalid = deepcopy(represented)
del invalid['types']['UA_Plugin']['representation']
rejected(contract=invalid, public=opaque_public, public_types=opaque_types, message='Unexplained C89 representation')
invalid = deepcopy(represented)
invalid['types']['UA_Plugin']['fields']['first'] = ['cpkt_opcua_PluginInfo.missing']
rejected(contract=invalid, public=opaque_public, public_types=opaque_types, message='Missing C89 access member')
invalid = deepcopy(represented)
invalid['types']['UA_Plugin']['fields']['first'] = []
rejected(contract=invalid, public=opaque_public, public_types=opaque_types, message='Missing public access')
invalid = deepcopy(represented)
invalid['types']['UA_Plugin']['fields']['first'] = ['cpkt_opcua_server_native']
rejected(contract=invalid, public=opaque_public | {'cpkt_opcua_server_native'}, public_types=opaque_types, message='Missing C89 access')
invalid = deepcopy(represented)
del invalid['types']['UA_Plugin']['fields']['first']
rejected(contract=invalid, public=opaque_public, public_types=opaque_types, message='Missing C89 record members')
invalid = deepcopy(represented)
invalid['types']['UA_Plugin']['fields']['nonexistent'] = ['cpkt_opcua_operation']
rejected(contract=invalid, public=opaque_public, public_types=opaque_types, message='Unknown represented native member')
changed = deepcopy(contract)
changed['functions']['UA_operation']['binding'] = 'cpkt_opcua_server_native'
rejected(contract=changed, public=public | {'cpkt_opcua_server_native'}, message='Native escape hatch')
changed = deepcopy(contract)
del changed['functions']['UA_implementation']['reason']
rejected(contract=changed, message='Unexplained implementation-only')
changed = deepcopy(contract)
changed['functions']['UA_operation']['state'] = 'silently_skipped'
rejected(contract=changed, message='Unknown coverage state')
changed = deepcopy(model)
del changed['functions']['UA_operation']
linux_contract = deepcopy(contract)
linux_contract['functions']['UA_operation']['platform'] = 'linux'
rejected(model=changed, contract=linux_contract, message='Missing contracted public functions')
api.check(changed, linux_contract, public, platform='darwin', public_types=public_types)

schema = api.extract('typedef unsigned int UA_UInt32; static inline void UA_UInt32_init(UA_UInt32 *p) {}')
empty = {category: {} for category in schema}
schema_public = {'cpkt_opcua_UInt32', 'cpkt_opcua_UInt32_init'}
assert len(api.check(schema, empty, schema_public, {'UInt32'})['bound']) == 2
schema['functions']['UA_UInt32_init'] = 'void UA_UInt32_init ( void * p )'
try:
    api.check(schema, empty, schema_public, {'UInt32'})
except ValueError as error:
    assert 'Changed generated public lifecycle declaration' in str(error)
else:
    raise AssertionError('Changed generated lifecycle signature was ignored')

# Contract decisions must be explicit, with no accidental native substitutions.
real = json.loads((repo / 'tools/opcua/public_api_contract.json').read_text())
assert all(rule['state'] in ('pending', 'bound', 'implementation')
           for category in ('functions', 'types', 'variables') for rule in real[category].values())
assert all(rule.get('reason') for category in ('functions', 'types', 'variables') for rule in real[category].values()
           if rule['state'] == 'implementation')
assert real['_origin']['headers']
assert api.enum_values('typedef enum { UA_A=0x80U, UA_B=UA_A+1, UA_C=-2, UA_D } UA_E;', 'UA_') == {
    'A': 128, 'B': 129, 'C': -2, 'D': -1}

if len(sys.argv) == 5:
    compiler, platform, build = sys.argv[2:]
    with tempfile.TemporaryDirectory(prefix='opcua-api-contract-', dir=build) as directory:
        root = Path(directory)
        native = root / 'native/open62541'
        facade = root / 'facade/cpkt'
        native.mkdir(parents=True)
        facade.mkdir(parents=True)
        native_text = '''typedef struct { void *context; void (*hook)(void *context); } UA_TestPlugin;
typedef enum { UA_TEST_FOO=0x80U, UA_TEST_BAR=UA_TEST_FOO+1 } UA_TestEnum;
void UA_test(void);
'''
        facade_text = native_text.replace('UA_TestPlugin', 'cpkt_opcua_TestPlugin').replace(
            'UA_TestEnum', 'cpkt_opcua_TestEnum').replace('UA_TEST_', 'CPKT_OPCUA_TEST_').replace(
            'UA_test', 'cpkt_opcua_test')
        (native / 'public.h').write_text(native_text)
        header = facade / 'opcua_types.h'
        header.write_text(facade_text)
        (root / 'facade/opcua_types_metadata.inc').write_text('/* No schema types in this fixture. */')
        mock = api.extract(native_text)
        bindings = {'UA_TestPlugin': 'cpkt_opcua_TestPlugin', 'UA_TestEnum': 'cpkt_opcua_TestEnum',
                    'UA_test': 'cpkt_opcua_test'}
        rules = {category: {name: {'declaration': declaration, 'state': 'bound', 'binding': bindings[name]}
                            for name, declaration in declarations.items()}
                 for category, declarations in mock.items()}
        contract_file = root / 'contract.json'
        contract_file.write_text(json.dumps(rules))
        report_file = root / 'report.json'
        command = [sys.executable, str(repo / 'tools/opcua/public_api.py'), '--compiler', compiler,
                   '--platform', platform, '--native-include', str(native.parent),
                   '--facade-include', str(facade.parent), '--generated-include', str(facade.parent),
                   '--contract', str(contract_file), '--report', str(report_file), '--require-complete']
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        assert not json.loads(report_file.read_text())['pending']
        rules['functions']['UA_test']['state'] = 'pending'
        contract_file.write_text(json.dumps(rules))
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode != 0 and 'coverage is incomplete' in result.stderr, result.stderr
        assert json.loads(report_file.read_text())['pending'] == ['UA_test']
        rules['functions']['UA_test']['state'] = 'bound'
        contract_file.write_text(json.dumps(rules))
        header.write_text(facade_text.replace('CPKT_OPCUA_TEST_FOO=0x80U', 'CPKT_OPCUA_TEST_FOO=1'))
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode != 0 and 'changed C89 enum constants' in result.stderr, result.stderr
        header.write_text(facade_text.replace('cpkt_opcua_test', 'cpkt_opcua_other'))
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode != 0 and 'Missing C89 binding' in result.stderr, result.stderr
print('New/changed API, opaque records, native escapes and missing bindings fail closed')
