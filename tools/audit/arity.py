"""Find calls that pass a different number of arguments than the callee has.

usage: arity.py CMDS AUX_DIR GIMPLE_DIR
CMDS is the list of compile commands (line i is translation unit i), AUX_DIR
holds i.aux from -aux-info, GIMPLE_DIR i-*.gimple from -fdump-tree-gimple.
Unprototyped (K&R) definitions and calls through "void f();" declarations
compile without complaint in C, so a missing argument is read from whatever
register or stack slot happens to hold it.
"""
import glob
import os
import re
import sys
from collections import defaultdict

DEF_RE = re.compile(r"^/\* (.*?):(\d+):([ON]F) \*/ (.*?) ([A-Za-z_]\w*) \((.*?)\); /\* \((.*?)\)")
CALL_RE = re.compile(r"^\s*(?:[\w.$]+ = )?([A-Za-z_]\w*) \((.*)\);\s*$")
FUNC_RE = re.compile(r"^([A-Za-z_]\w*) \((.*)\)\s*$")


def split_args(s):
    s = s.strip()
    if not s:
        return []
    out, depth, cur, q = [], 0, "", None
    for ch in s:
        if q:
            cur += ch
            if ch == q:
                q = None
            continue
        if ch in "\"'":
            q = ch
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            out.append(cur)
            cur = ""
            continue
        cur += ch
    out.append(cur)
    return out


def main():
    cmds_path, aux_dir, gim_dir = sys.argv[1:4]
    srcs = {}
    with open(cmds_path) as f:
        for i, line in enumerate(f, 1):
            m = re.search(r"-c '([^']*)'", line)
            srcs[i] = m.group(1) if m else "?"
    defs = defaultdict(list)  # name -> [(tu, arity, varargs, where)]
    for path in glob.glob(os.path.join(aux_dir, "*.aux")):
        tu = int(os.path.basename(path).split(".")[0])
        with open(path, errors="replace") as f:
            for line in f:
                m = DEF_RE.match(line)
                if not m:
                    continue
                where, lineno, kind, ret, name, proto, names = m.groups()
                if "/game/" not in where and "port/game" not in where:
                    continue
                varargs = "..." in proto
                names = names.strip()
                arity = 0 if names in ("", "void") else len(names.split(","))
                entry = (tu, arity, varargs, "%s:%s" % (where.split("dw1-port/")[-1], lineno), "static" in ret)
                if entry not in defs[name]:
                    defs[name].append(entry)
    seen = set()
    for path in sorted(glob.glob(os.path.join(gim_dir, "*.gimple"))):
        tu = int(os.path.basename(path).split("-")[0])
        caller = "?"
        with open(path, errors="replace") as f:
            for line in f:
                fm = FUNC_RE.match(line)
                if fm and not line.startswith(" "):
                    caller = fm.group(1)
                    continue
                cm = CALL_RE.match(line)
                if not cm:
                    continue
                name, args = cm.groups()
                if name.startswith("__builtin") or name not in defs:
                    continue
                cands = [d for d in defs[name] if d[0] == tu] or [d for d in defs[name] if not d[4]]
                if not cands:
                    continue
                n = len(split_args(args))
                if any(d[2] or d[1] == n for d in cands):
                    continue
                key = (srcs[tu], caller, name, n)
                if key in seen:
                    continue
                seen.add(key)
                want = "/".join(str(d[1]) for d in cands)
                print("%s: %s() calls %s() with %d args, defined with %s (%s)" % (
                    srcs[tu].split("dw1-port/")[-1], caller, name, n, want, cands[0][3]))


if __name__ == "__main__":
    main()
