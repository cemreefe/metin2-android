# Minimal py2-compatible posixpath for the embedded wasm interpreter.
# Upstream m2dev-client ships a trimmed lib/ that lacks this module, so
# `import os` died on `import posixpath`. Only the API os.pyc and the game
# actually use is implemented (pure string ops, POSIX semantics).

from genericpath import exists, isfile, isdir, getsize, getmtime, getatime, getctime

sep = '/'
pathsep = ':'
defpath = ':/bin:/usr/bin'
curdir = '.'
pardir = '..'
extsep = '.'
altsep = None
devnull = '/dev/null'

import posix


def normcase(s):
    return s


def isabs(s):
    return s[:1] == '/'


def join(a, *p):
    path = a
    for b in p:
        if b[:1] == '/':
            path = b
        elif path == '' or path[-1:] == '/':
            path += b
        else:
            path += '/' + b
    return path


def split(p):
    i = p.rfind('/') + 1
    head, tail = p[:i], p[i:]
    if head and head != '/' * len(head):
        head = head.rstrip('/')
    return head, tail


def splitext(p):
    i = p.rfind('.')
    j = max(p.rfind('/'), p.rfind('\\'))
    if i > j:
        return p[:i], p[i:]
    return p, ''


def basename(p):
    i = p.rfind('/') + 1
    return p[i:]


def dirname(p):
    i = p.rfind('/') + 1
    head = p[:i]
    if head and head != '/' * len(head):
        head = head.rstrip('/')
    return head


def normpath(path):
    if path == '':
        return '.'
    initial_slashes = path.startswith('/')
    if initial_slashes and path.startswith('//') and not path.startswith('///'):
        initial_slashes = 2
    comps = path.split('/')
    new_comps = []
    for comp in comps:
        if comp in ('', '.'):
            continue
        if comp != '..' or (not initial_slashes and not new_comps) or \
           (new_comps and new_comps[-1] == '..'):
            new_comps.append(comp)
        elif new_comps:
            new_comps.pop()
    comps = new_comps
    path = '/'.join(comps)
    if initial_slashes:
        path = '/' * initial_slashes + path
    return path or '.'


def abspath(path):
    if not isabs(path):
        path = join(posix.getcwd(), path)
    return normpath(path)


realpath = abspath


def lexists(path):
    try:
        posix.lstat(path)
        return True
    except:
        return False


def islink(path):
    try:
        st = posix.lstat(path)
    except:
        return False
    import stat
    return stat.S_ISLNK(st.st_mode)


def samefile(f1, f2):
    return normcase(abspath(f1)) == normcase(abspath(f2))


def expanduser(path):
    if path[:1] != '~':
        return path
    import pwd
    i = path.find('/', 1)
    if i < 0:
        i = len(path)
    if i == 1:
        userhome = posix.environ.get('HOME')
        if not userhome:
            try:
                userhome = pwd.getpwuid(posix.getuid()).pw_dir
            except:
                return path
    else:
        try:
            userhome = pwd.getpwnam(path[1:i]).pw_dir
        except:
            return path
    return userhome + path[i:]


def expandvars(path):
    if '$' not in path:
        return path
    environ = posix.environ
    i = 0
    out = ''
    while i < len(path):
        c = path[i]
        if c == '$':
            j = i + 1
            if j < len(path) and path[j] == '{':
                k = path.find('}', j)
                if k < 0:
                    out += c
                    i += 1
                    continue
                out += environ.get(path[j + 1:k], '')
                i = k + 1
            else:
                k = j
                while k < len(path) and (path[k].isalnum() or path[k] == '_'):
                    k += 1
                out += environ.get(path[j:k], '')
                i = k
        else:
            out += c
            i += 1
    return out


def commonprefix(m):
    if not m:
        return ''
    s1 = min(m)
    s2 = max(m)
    for i, c in enumerate(s1):
        if c != s2[i]:
            return s1[:i]
    return s1


def walk(top, func, arg):
    try:
        names = posix.listdir(top)
    except:
        return
    func(arg, top, names)
    for name in names:
        name = join(top, name)
        if isdir(name) and not islink(name):
            walk(name, func, arg)
