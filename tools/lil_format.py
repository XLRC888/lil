#!/usr/bin/env python3
import sys
import re

KW_SET = {
    "print",
    "input",
    "if",
    "else",
    "elif",
    "while",
    "for",
    "to",
    "exit",
    "and",
    "or",
    "not",
    "loop",
    "stop",
    "include",
    "function",
    "has",
    "nocase",
    "anywhere",
    "word",
}
BOUNDED = {"if", "in", "to", "or", "not", "for", "and", "has"}


def mk_kw_pat():
    parts = []
    for kw in sorted(KW_SET, key=len, reverse=True):
        if kw in BOUNDED:
            parts.append(r"\b" + re.escape(kw) + r"\b")
        else:
            parts.append(re.escape(kw))
    return "|".join(parts)


RE = re.compile(
    r"""
    (?P<COMMENT>\#.*)
    |(?P<TEMPLATE>`(?:[^`\\]|\\.)*`)
    |(?P<STRING>"(?:[^"\\]|\\.)*")
    |(?P<NUM>\d+\.?\d*(?:[eE][+-]?\d+)?)
    |(?P<KW>"""
    + mk_kw_pat()
    + r""")
    |(?P<OP>&&|\|\||==|!=|<=|>=|[-+*/%<>&|!]=?|=)
    |(?P<LP>\()
    |(?P<RP>\))
    |(?P<LB>\{)
    |(?P<RB>\})
    |(?P<LS>\[)
    |(?P<RS>\])
    |(?P<CM>,)
    |(?P<SC>;)
    |(?P<DL>\$\w*)
    |(?P<ID>[a-zA-Z_]\w*)
    |(?P<NL>\n)
    |(?P<SKIP>[ \t\r]+)
    |(?P<BAD>.)
""",
    re.VERBOSE | re.MULTILINE,
)


def tok(src):
    return [
        (m.lastgroup, m.group()) for m in RE.finditer(src) if m.lastgroup != "SKIP"
    ] + [("EOF", "")]


def fmt(src):
    toks = tok(src)
    out = ""
    ind = 0
    SP = "    "
    sol = True
    i = 0
    n = len(toks)
    prev_k = None
    prev_v = None
    pending = 0

    def e(s):
        nonlocal out, sol
        if sol:
            out += SP * ind + s
        else:
            out += s
        sol = False

    def nl_():
        nonlocal out, sol
        out += "\n"
        sol = True

    def drain():
        nonlocal pending
        if pending == 0:
            return
        if not sol:
            nl_()
            pending -= 1
        while pending > 0:
            nl_()
            pending -= 1

    def sp():
        nonlocal out
        if out and not out[-1].isspace() and out[-1] != "(" and out[-1] != "[":
            out += " "

    while i < n:
        k, v = toks[i]

        if k == "EOF":
            break

        if k == "NL":
            pending += 1
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "COMMENT":
            drain()
            if sol:
                e(v)
            else:
                out += "  " + v
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "RB":
            ind = max(0, ind - 1)
            j = i + 1
            while j < n and toks[j][0] == "NL":
                j += 1
            fol = toks[j] if j < n else None
            if fol and fol[0] == "KW" and fol[1] in ("else", "elif"):
                drain()
                if not sol:
                    nl_()
                e("}")
                if fol[1] == "elif":
                    out += " elif "
                else:
                    out += " else"
                    kk = j + 1
                    if kk < n and toks[kk][0] == "KW" and toks[kk][1] == "if":
                        out += " if "
                        i = kk + 1
                        prev_k = k
                        prev_v = v
                        continue
                    out += " "
                i = j + 1
                prev_k = k
                prev_v = v
                continue
            drain()
            if not sol:
                nl_()
            e("}")
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "LB":
            if sol:
                e("{")
            else:
                out = out.rstrip() + " {"
            ind += 1
            sol = False
            j = i + 1
            while j < n and toks[j][0] == "NL":
                j += 1
            fol = toks[j] if j < n else None
            if fol and fol[0] == "RB":
                nl_()
                ind -= 1
                e("}")
                i = j + 1
                prev_k = k
                prev_v = v
                continue
            if j > i + 1:
                pending = j - i - 1
                i = j
                prev_k = k
                prev_v = v
                continue
            if i + 1 < n and toks[i + 1][0] != "NL":
                nl_()
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "KW":
            drain()
            if v in ("and", "or"):
                sp()
                out += v + " "
                sol = False
            elif v == "not":
                if sol:
                    e(v + " ")
                else:
                    sp()
                    out += v + " "
                    sol = False
            elif v == "has":
                sp()
                out += v + " "
                sol = False
            elif v in ("nocase", "anywhere", "word"):
                sp()
                out += v
                sol = False
            elif v == "to":
                sp()
                out += v + " "
                sol = False
            elif v in ("else", "elif"):
                if not sol:
                    nl_()
                e(v + " ")
            elif v == "loop":
                if not sol:
                    nl_()
                e(v)
            elif v in ("if", "while", "for"):
                if not sol:
                    nl_()
                e(v + " ")
            elif v in ("exit", "stop"):
                if not sol:
                    nl_()
                e(v)
            elif v == "function":
                if sol:
                    e(v)
                else:
                    out += v
                sol = False
            else:
                if not sol:
                    nl_()
                e(v + " ")
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "CM":
            drain()
            out = out.rstrip() + ", "
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "OP":
            drain()
            if v == "=":
                sp()
                out += "= "
                sol = False
            elif v == "-" and (
                sol
                or prev_k in ("LP", "CM", "LS")
                or prev_v
                in (
                    "and",
                    "or",
                    "not",
                    "has",
                    "to",
                    "function",
                    "print",
                    "+",
                    "-",
                    "*",
                    "/",
                    "%",
                    "==",
                    "!=",
                    "<",
                    ">",
                    "<=",
                    ">=",
                    "&&",
                    "||",
                    "=",
                )
            ):
                out += "-"
                sol = False
            elif v == "!" and (sol or prev_k in ("LP", "CM", "LS")):
                out += "!"
                sol = False
            else:
                sp()
                out += v + " "
                sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k in ("LP", "LS"):
            drain()
            out += v
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k in ("RP", "RS"):
            drain()
            out += v
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "SC":
            out += ";"
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "DL":
            drain()
            sp()
            out += v
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k in ("ID", "NUM", "STRING", "TEMPLATE"):
            drain()
            if sol:
                e(v)
            else:
                gap = (
                    prev_k in ("ID", "NUM", "STRING", "TEMPLATE", "RP", "RS", "DL")
                    or (
                        prev_k == "KW"
                        and prev_v
                        in (
                            "exit",
                            "stop",
                            "function",
                        )
                    )
                    or prev_v == "loop"
                )
                if gap:
                    out += " "
                out += v
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        if k == "BAD":
            drain()
            if sol:
                e(v)
            else:
                out += v
            sol = False
            prev_k = k
            prev_v = v
            i += 1
            continue

        i += 1

    out = out.rstrip("\n") + "\n"

    lines = out.split("\n")
    clean = []
    blanks = 0
    for L in lines:
        if L.strip() == "":
            blanks += 1
            if blanks <= 2:
                clean.append("")
        else:
            blanks = 0
            clean.append(L.rstrip())
    while clean and clean[-1] == "":
        clean.pop()

    return "\n".join(clean) + "\n"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check = "--check" in sys.argv

    if len(args) < 1:
        sys.stdout.write(fmt(sys.stdin.read()))
        return

    for path in args:
        with open(path, "r") as f:
            src = f.read()

        formatted = fmt(src)

        if check:
            if src != formatted:
                print(f"{path}: would reformat")
                sys.exit(1)
        else:
            with open(path, "w") as f:
                f.write(formatted)


if __name__ == "__main__":
    main()
