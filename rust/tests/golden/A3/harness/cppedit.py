import re


def rd(p):
    return open(p, encoding='utf-8', errors='surrogateescape').read()


def wr(p, t):
    open(p, 'w', encoding='utf-8', errors='surrogateescape').write(t)


def span(t, sig, nth=0):
    ms = list(re.finditer(r'^(?:[A-Za-z_][^\n]*\b)?' + re.escape(sig) + r'\(', t, re.M))
    m = ms[nth]
    i = m.start()
    j = re.compile(r'\n\{[ \t]*\n').search(t, i).start()
    d, k = 0, j + 1
    while True:
        c = t[k]
        d += c == '{'
        d -= c == '}'
        k += 1
        if d == 0:
            break
    return i, k


def repl(t, sig, new, nth=0):
    i, k = span(t, sig, nth)
    return t[:i] + new.rstrip('\n') + t[k:]


def delete(t, sig, nth=0):
    i, k = span(t, sig, nth)
    return t[:i] + t[k:].lstrip('\n')
