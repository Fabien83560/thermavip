#!/usr/bin/env python
"""Check the two properties of Thermavip.py that do not need an interpreter
running inside the application: the buffers written through ctypes are mutable
and private, and the two public wrappers pass a tuple."""
import ast
import ctypes
import os
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Python', 'Thermavip.py')
text = open(SRC, encoding='utf-8').read()
tree = ast.parse(text)

failures = []


def check(label, condition, detail=''):
    if not condition:
        failures.append('%s%s' % (label, (': ' + detail) if detail else ''))


# 1. every destination of ctypes.memmove that is a local name must have been
#    built by create_string_buffer, never by a bytes literal.
literal_dest = []
for node in ast.walk(tree):
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == 'memmove':
        dest = node.args[0]
        if isinstance(dest, ast.Name):
            literal_dest.append(dest.id)

assigned_by_buffer = set()
for node in ast.walk(tree):
    if isinstance(node, ast.Assign) and isinstance(node.value, ast.Call):
        fn = node.value.func
        name = fn.attr if isinstance(fn, ast.Attribute) else getattr(fn, 'id', '')
        if name == 'create_string_buffer':
            for t in node.targets:
                if isinstance(t, ast.Name):
                    assigned_by_buffer.add(t.id)

for name in literal_dest:
    check('the destination %r of a memmove is a mutable buffer' % name, name in assigned_by_buffer)

# 2. writing into a bytes constant is what the old code did: show that such a
#    constant is shared, so the test above is not academic.
def one():
    return b'\x00' * 8


def two():
    return b'\x00' * 8


check('two identical bytes constants are the same object', one() is two(),
      'the folding this depends on is not happening in this interpreter')

# 3. the two wrappers must pass a tuple, not a parenthesised value.
for fun in ('x_range', 'remove_annotation'):
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == fun:
            calls = [n for n in ast.walk(node) if isinstance(n, ast.Call)
                     and getattr(n.func, 'id', '') == 'call_thermavip_fun']
            check('%s calls the dispatcher' % fun, len(calls) == 1)
            if calls:
                arg = calls[0].args[1]
                check('%s passes a tuple' % fun, isinstance(arg, ast.Tuple),
                      'passes %s' % type(arg).__name__)
            break
    else:
        check('%s exists' % fun, False)

# 4. no strict ascii codec is left: a single accented character used to abort a
#    send from inside an exception handler, with the lock held.
for node in ast.walk(tree):
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr in ('encode', 'decode'):
        has_errors = any(k.arg == 'errors' for k in node.keywords)
        check('the codec at line %d says what to do with what it cannot encode' % node.lineno, has_errors)

# 5. the line of code must travel in the event, not in an attribute shared
#    with the thread that reads the next message.
for node in ast.walk(tree):
    if isinstance(node, ast.FunctionDef) and node.name == 'callLine':
        check('callLine takes the code as an argument', len(node.args.args) == 2,
              'takes %d argument(s)' % len(node.args.args))
        break
else:
    check('callLine exists', False)

for node in ast.walk(tree):
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == 'invokeMethod':
        if len(node.args) > 1 and isinstance(node.args[1], ast.Constant) and node.args[1].value == 'callLine':
            check('the call to callLine at line %d carries its code' % node.lineno, len(node.args) > 3,
                  'passes %d argument(s)' % len(node.args))

# 6. the two waits on the segment have a finite default: they are held under the
#    lock the service thread needs, so an endless one freezes the console.
for fun in ('read', 'write'):
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == fun:
            defaults = node.args.defaults
            check('%s has a default timeout' % fun, bool(defaults))
            if defaults:
                value = defaults[-1]
                check('the default timeout of %s is finite' % fun,
                      isinstance(value, ast.Constant) and isinstance(value.value, int) and value.value > 0,
                      'is %r' % getattr(value, 'value', value))
            break
    else:
        check('%s exists' % fun, False)

if failures:
    for f in failures:
        print('FAIL:', f)
    sys.exit(1)
print('all Thermavip.py checks passed')
