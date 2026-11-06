#!/usr/bin/env python3
"""Interface-method coverage: which COM methods are called through the C macros (ID3D11Device_CreateBuffer(...)) or lpVtbl->Method.
usage: method_coverage.py [corpus_dir] [sdk_include_dir]
The method list is read from the SDK headers (default /usr/x86_64-w64-mingw32/include, the mingw-w64 headers the corpus builds with).
A method name counts as covered when it is called through any interface (inherited methods share the name). Prints the never-called
method names per interface family; use it to find gaps. A static scan: calls through tables of function pointers stay invisible."""
import re, sys, glob, os, collections
root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
inc = (sys.argv[2] if len(sys.argv) > 2 else "/usr/x86_64-w64-mingw32/include").rstrip("/") + "/"
hdrs = ("d3d9 d3d10 d3d10_1 d3d11 d3d11_1 d3d11_2 d3d11_3 d3d11_4 d3d11on12 d3d11sdklayers d3d10sdklayers d3d12 d3d12sdklayers d3d12video "
        "dxgi dxgi1_2 dxgi1_3 dxgi1_4 dxgi1_5 dxgi1_6 dxgidebug d3dcommon d3dcompiler d3d11shader d3d12shader d3d10shader d3d10effect ddraw dxva2api d3d10misc").split()
src = ""
for f in glob.glob(os.path.join(root, "tests", "**", "*.[ch]"), recursive=True) + glob.glob(os.path.join(root, "common", "*")):
    src += open(f, errors="replace").read() + "\n"
called = {c.split("_", 1)[1] for c in re.findall(r'\b(I[A-Za-z0-9]+_[A-Za-z0-9]+)\s*\(', src)}
called |= set(re.findall(r'->\s*lpVtbl\s*->\s*(\w+)', src))
decl = collections.OrderedDict()
for h in hdrs:
    try:
        t = open(inc + h + ".h", errors="replace").read()
    except OSError:
        continue
    for m in re.finditer(r'^#define (I[A-Za-z0-9]+)_([A-Za-z0-9]+)\(This', t, re.M):
        i, meth = m.groups()
        if meth in ("QueryInterface", "AddRef", "Release"):
            continue
        decl.setdefault(meth, i)          # first (lowest version) interface that declares the name
fam = collections.defaultdict(list)
for meth, i in decl.items():
    if meth not in called:
        fam[i].append(meth)
total = len(decl); missing = sum(len(v) for v in fam.values())
print(f"{total} distinct method names; {total - missing} called; {missing} never called")
for i in sorted(fam):
    print(f"{i}: {', '.join(sorted(fam[i]))}")
