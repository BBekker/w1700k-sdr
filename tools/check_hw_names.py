#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Every macro name under include/hw/ is defined exactly once.

Parses every header (comments stripped, continuation lines joined) and reports
each name #defined in more than one place, saying whether the definitions are
textually identical. The compiler accepts an identical redefinition silently,
so this is the only thing that keeps one name in one header.

    tools/check_hw_names.py [include/hw]    exit 0: no duplicates; 1: duplicates
"""
import os
import re
import sys

DEFINE = re.compile(r'#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?(.*)', re.S)
UNDEF = re.compile(r'#\s*undef\s+([A-Za-z_]\w*)')


def strip_comments(text):
    """Replace comments with spaces, keeping newlines so line numbers hold."""
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
        elif text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif text[i] in '"\'':
            q, j = text[i], i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return ''.join(out)


def directives(path):
    """(line, directive text) for each preprocessor line, continuations joined."""
    lines = strip_comments(open(path, encoding='utf-8').read()).split('\n')
    i = 0
    while i < len(lines):
        start, cur = i, lines[i]
        while cur.endswith('\\') and i + 1 < len(lines):
            i += 1
            cur = cur[:-1] + ' ' + lines[i]
        if cur.lstrip().startswith('#'):
            yield start + 1, cur.strip()
        i += 1


def scan(root):
    defs = {}
    for d, _, files in sorted(os.walk(root)):
        for f in sorted(files):
            if not f.endswith('.h'):
                continue
            path = os.path.join(d, f)
            rel = os.path.relpath(path, root)
            for line, text in directives(path):
                if UNDEF.match(text):
                    sys.exit(f'{rel}:{line}: #undef is not used in these headers: {text}')
                m = DEFINE.match(text)
                if m:
                    body = (m.group(2) or '') + ' '.join(m.group(3).split())
                    defs.setdefault(m.group(1), []).append((rel, line, body))
    return defs


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', 'include', 'hw')
    defs = scan(root)
    dups = {k: v for k, v in defs.items() if len(v) > 1}
    for name in sorted(dups):
        where = dups[name]
        kind = 'identical' if len({b for _, _, b in where}) == 1 else 'DIFFERENT'
        print(f'{name}: {len(where)} definitions, {kind}')
        for rel, line, body in where:
            print(f'    {rel}:{line}  {body[:100]}')
    print(f'{len(defs)} names in {root}; {len(dups)} defined more than once')
    return 1 if dups else 0


if __name__ == '__main__':
    sys.exit(main())
