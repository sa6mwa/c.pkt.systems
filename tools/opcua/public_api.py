#!/usr/bin/env python3
"""Inventory the enabled public SDK declarations with the configured compiler.

This is a coverage gate, not a second OPC UA schema parser. It consumes C
preprocessor output and ignores function bodies. The upstream schema generator
continues to own schema filtering and type generation. Build products and
reports belong under build/; the checked-in contract records binding decisions.
"""
import argparse
import ast
import json
import operator
from pathlib import Path
import re
import subprocess

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|\d+(?:\.\d+)?|\S')
FUNCTION = re.compile(r'\b([A-Za-z_]\w*)\s*\((?!\s*\*)')


def declarations(text):
    """Keep complete top-level declarations and function-definition prefixes."""
    current, output = [], []
    depth, body = 0, False
    for token in TOKEN.findall(text):
        if token == '{':
            if not depth:
                body = (current and current[0] != 'typedef' and
                        FUNCTION.search(canonical(current)) is not None)
                if body:
                    output.append(current)
                    current = []
            depth += 1
        elif token == '}':
            depth -= 1
            if depth < 0:
                raise ValueError('Unbalanced preprocessed public declaration')
        if not body:
            current.append(token)
        if not depth:
            if token == ';':
                output.append(current)
                current = []
            elif token == '}' and body:
                body = False
                current = []
    if depth or current:
        raise ValueError('Incomplete preprocessed public declaration')
    return output


def canonical(tokens):
    """Ignore linkage/compiler attributes; keep types, fields and array bounds."""
    result = []
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token in ('__attribute__', '__attribute', '__declspec'):
            i += 1
            if i >= len(tokens) or tokens[i] != '(':
                raise ValueError('Unsupported compiler attribute')
            depth = 0
            while i < len(tokens):
                if tokens[i] == '(':
                    depth += 1
                elif tokens[i] == ')':
                    depth -= 1
                i += 1
                if not depth:
                    break
            if depth:
                raise ValueError('Unbalanced compiler attribute')
            continue
        if token not in ('static', 'extern', 'inline', '__inline__', '__inline', ';'):
            result.append(token)
        elif token == ';' and i != len(tokens) - 1:
            result.append(token)
        i += 1
    return ' '.join(result)


def extract(text):
    functions, types, variables = {}, {}, {}
    for tokens in declarations(text):
        spelling = ' '.join(tokens)
        if not tokens:
            continue
        if tokens[0] == 'typedef':
            # Function-pointer typedefs name the alias between (* and ).
            callback = (None if tokens[1] in ('struct', 'union', 'enum') else
                        re.search(r'\( \* ([A-Za-z_]\w*) \)', spelling))
            name = callback[1] if callback else tokens[-2]
            if re.fullmatch(r'static_assertion_failed_\d+', name):
                # Expansions of UA_STATIC_ASSERT enforce native representation
                # at compile time; they are not consumer datatype declarations.
                continue
            value = canonical(tokens)
            if name not in types or '{' in tokens or '{' not in types[name]:
                types[name] = value
        elif tokens[0] in ('struct', 'union', 'enum'):
            if '{' in tokens:
                types[tokens[1]] = canonical(tokens)
            else:
                types.setdefault(tokens[1], canonical(tokens))
        else:
            names = FUNCTION.findall(canonical(tokens))
            if names:
                if len(names) != 1:
                    raise ValueError(f'Ambiguous public function declaration: {spelling}')
                name = names[0]
                value = canonical(tokens)
                if name in functions and functions[name] != value:
                    raise ValueError(f'Conflicting public declarations: {name}')
                functions[name] = value
            else:
                variable = re.search(r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)?;$', spelling)
                if variable:
                    variables[variable[1]] = canonical(tokens)
                else:
                    raise ValueError(f'Unclassified public declaration: {spelling}')
    return {'functions': dict(sorted(functions.items())),
            'types': dict(sorted(types.items())),
            'variables': dict(sorted(variables.items()))}


def preprocess(compiler, include, headers, extra_includes=(), namespace='open62541'):
    source = ''.join(f'#include <{header}>\n' for header in headers)
    roots = (include, *extra_includes)
    command = [compiler, '-x', 'c', '-std=c99', '-E']
    for root in roots:
        command += ['-I', str(root)]
    result = subprocess.run(command + ['-'], input=source,
                            capture_output=True, text=True)
    if result.returncode:
        raise ValueError(f'Public-header preprocessing failed:\n{result.stderr}')
    # Retain SDK-owned declarations, including the history plugin's deliberately
    # unprefixed MatchStrategy. Do not mistake system headers for public SDK API.
    owned = [(root / namespace).resolve() for root in roots]
    active, lines = False, []
    for line in result.stdout.splitlines():
        marker = re.match(r'^#\s+\d+\s+"([^"]+)"', line)
        if marker:
            active = False
            for root in owned:
                try:
                    Path(marker[1]).resolve().relative_to(root)
                    active = True
                    break
                except ValueError:
                    pass
        elif active:
            lines.append(line)
    return '\n'.join(lines)


def record_fields(declaration):
    """Public member names, including callbacks, unions, arrays and bitfields."""
    tokens = TOKEN.findall(declaration)
    if '{' not in tokens or 'enum' in tokens[:tokens.index('{')]:
        return set()
    start = tokens.index('{') + 1
    depth, current, fields = 0, [], set()
    for token in tokens[start:]:
        if token == '}' and not depth:
            break
        if token in ('{', '(', '['):
            depth += 1
        elif token in ('}', ')', ']'):
            depth -= 1
        if token in (';', ',') and not depth:
            spelling = ' '.join(current)
            callback = re.search(r'\( \* ([A-Za-z_]\w*) \)', spelling)
            field = callback or re.search(r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)?(?::\s*.*)?$', spelling)
            if not field:
                raise ValueError(f'Unsupported public record member: {spelling}')
            fields.add(field[1])
            current = []
        else:
            current.append(token)
    return fields


def enum_values(declaration, prefix):
    """Evaluate integer enum constants without executing header expressions."""
    tokens = TOKEN.findall(declaration)
    if 'enum' not in tokens or '{' not in tokens:
        return None
    body = ''.join(tokens[tokens.index('{') + 1:tokens.index('}')])
    values, previous = {}, -1
    binary = {ast.Add: operator.add, ast.Sub: operator.sub,
              ast.Mult: operator.mul, ast.LShift: operator.lshift,
              ast.RShift: operator.rshift, ast.BitOr: operator.or_,
              ast.BitAnd: operator.and_, ast.BitXor: operator.xor}
    unary = {ast.USub: operator.neg, ast.UAdd: operator.pos, ast.Invert: operator.invert}

    def integer(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name) and node.id in values:
            return values[node.id]
        if isinstance(node, ast.BinOp) and type(node.op) in binary:
            return binary[type(node.op)](integer(node.left), integer(node.right))
        if isinstance(node, ast.UnaryOp) and type(node.op) in unary:
            return unary[type(node.op)](integer(node.operand))
        raise ValueError('Unsupported public enum expression')

    for entry in body.split(','):
        if not entry:
            continue
        name, separator, expression = entry.partition('=')
        if separator:
            expression = re.sub(r'\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b', r'\1', expression)
            previous = integer(ast.parse(expression, mode='eval').body)
        else:
            previous += 1
        values[name] = previous
    if any(not name.startswith(prefix) for name in values):
        raise ValueError('Unsupported public enum constant prefix: ' + prefix)
    return {name[len(prefix):]: value for name, value in values.items()}


def check(native, contract, public, schema=(), platform='linux', public_types=None):
    failures, pending, bound, excluded = [], [], [], []
    for category in ('functions', 'types', 'variables'):
        expected = contract[category]
        observed = {}
        for name, declaration in native[category].items():
            lifecycle = re.fullmatch(r'UA_(\w+)_(init|new|copy|clear|delete|equal)', name)
            generated = ((category == 'types' and name[3:] in schema) or
                         (category == 'functions' and lifecycle and lifecycle[1] in schema))
            if generated:
                binding = name.replace('UA_', 'cpkt_opcua_', 1)
                if binding not in public:
                    failures.append(f'Missing generated C89 binding: {binding}')
                if category == 'functions':
                    typename, action = lifecycle.groups()
                    spelling = {
                        'init': f'void {name} ( UA_{typename} * p )',
                        'new': f'UA_{typename} * {name} ( void )',
                        'copy': f'UA_StatusCode {name} ( const UA_{typename} * src , UA_{typename} * dst )',
                        'clear': f'void {name} ( UA_{typename} * p )',
                        'delete': f'void {name} ( UA_{typename} * p )',
                        'equal': f'UA_Boolean {name} ( const UA_{typename} * p1 , const UA_{typename} * p2 )',
                    }[action]
                    if declaration != spelling:
                        failures.append(f'Changed generated public lifecycle declaration: {name}')
                bound.append(name)
            else:
                observed[name] = declaration
        for name in sorted(observed.keys() - expected.keys()):
            failures.append(f'Unclassified public {category}: {name}')
        for name in sorted(expected.keys() - observed.keys()):
            rule = expected[name]
            if rule.get('platform') != 'linux' or platform == 'linux':
                failures.append(f'Missing contracted public {category}: {name}')
        for name in sorted(observed.keys() & expected.keys()):
            rule = expected[name]
            if rule['declaration'] != observed[name]:
                failures.append(f'Changed public {category} declaration: {name}\n'
                                f"  expected: {rule['declaration']}\n"
                                f'  observed: {observed[name]}')
            state = rule['state']
            if state == 'pending':
                pending.append(name)
            elif state == 'bound':
                binding = rule.get('binding')
                if binding and re.search(r'_native(?:_|$)', binding):
                    failures.append(f'Native escape hatch is not a C89 binding: {name}')
                if not binding or binding not in public:
                    failures.append(f'Missing C89 binding for {name}: {binding}')
                if category == 'types' and public_types is not None:
                    missing = record_fields(observed[name]) - record_fields(public_types.get(binding, ''))
                    if missing:
                        failures.append(f'Missing C89 record members for {name}: {sorted(missing)}')
                    native_prefix = rule.get('native_constants', 'UA_' if name.startswith('UA_') else '')
                    constants = enum_values(observed[name], native_prefix)
                    if constants is not None:
                        exposed = enum_values(public_types.get(binding, ''), rule.get('public_constants', 'CPKT_OPCUA_'))
                        if constants != exposed:
                            failures.append(f'Missing or changed C89 enum constants for {name}')
                bound.append(name)
            elif state == 'implementation':
                if not rule.get('reason'):
                    failures.append(f'Unexplained implementation-only declaration: {name}')
                excluded.append(name)
            else:
                failures.append(f'Unknown coverage state for {name}: {state}')
    if failures:
        raise ValueError('\n'.join(failures))
    return {'bound': bound, 'pending': pending, 'implementation': excluded}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--platform', required=True, choices=('linux', 'darwin'))
    parser.add_argument('--native-include', type=Path, required=True)
    parser.add_argument('--facade-include', type=Path, required=True)
    parser.add_argument('--generated-include', type=Path, required=True)
    parser.add_argument('--contract', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--require-complete', action='store_true')
    args = parser.parse_args()
    headers = sorted('open62541/' + str(p.relative_to(args.native_include / 'open62541'))
                     for p in (args.native_include / 'open62541').rglob('*.h'))
    native = extract(preprocess(args.compiler, args.native_include, headers))
    # Bindings are checked against actual declarations/macros in shipped C89
    # headers. Native escape-hatch implementations never count as bindings.
    facade = extract(preprocess(args.compiler, args.facade_include,
                     ['cpkt/opcua_types.h'], [args.generated_include], 'cpkt'))
    public = set().union(*[set(values) for values in facade.values()])
    for root in (args.facade_include, args.generated_include):
        for header in (root / 'cpkt').glob('opcua*.h'):
            text = re.sub(r'/\*.*?\*/', '', header.read_text(), flags=re.S)
            public.update(re.findall(r'^\s*#define\s+(cpkt_opcua_\w+)\s*\(', text, re.M))
    metadata = (args.generated_include / 'opcua_types_metadata.inc').read_text()
    schema = set(re.findall(r'^  \{"(\w+)", sizeof\(cpkt_opcua_', metadata, re.M))
    report = check(native, json.loads(args.contract.read_text()), public, schema,
                   args.platform, facade['types'])
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Public OPC UA coverage: {len(report['bound'])} bound, "
          f"{len(report['pending'])} pending, "
          f"{len(report['implementation'])} implementation-only declarations")
    if args.require_complete and report['pending']:
        raise ValueError('Public C89 coverage is incomplete; see ' + str(args.report))


if __name__ == '__main__':
    try:
        main()
    except ValueError as error:
        raise SystemExit(str(error)) from error
