#!/usr/bin/env python3
"""Convert m2dev-server sql/*.sql (mysqldump/MariaDB) into SQLite databases
used by the embedded server's SQLite-backed MySQL shim.

    mysql2sqlite.py <m2dev-server/sql> <out_dir> [--test-account LOGIN:PASSWORD]

Produces <out_dir>/{account,player,common,log}.sqlite3. Enum/set columns are
stored as TEXT; their definitions go into the per-database _m2_enum table so
the shim can emulate `col+0`, and triggers normalise integer writes into the
matching enum text the way MySQL does.
"""
import argparse
import hashlib
import os
import re
import sqlite3
import sys

DATABASES = ["account", "player", "common", "log"]


def mysql_password(pw: str) -> str:
    return "*" + hashlib.sha1(hashlib.sha1(pw.encode()).digest()).hexdigest().upper()


# ------------------------------------------------------------------ lexing

def split_statements(data: bytes):
    """Split a dump into statements, honouring quotes and comments."""
    out, cur, i, n = [], bytearray(), 0, len(data)
    while i < n:
        c = data[i:i + 1]
        if c in (b"'", b'"', b"`"):
            q = c
            cur += c
            i += 1
            while i < n:
                d = data[i:i + 1]
                cur += d
                if d == b"\\" and q != b"`":
                    cur += data[i + 1:i + 2]
                    i += 2
                    continue
                i += 1
                if d == q:
                    break
            continue
        if data.startswith(b"--", i) and (i + 2 >= n or data[i + 2:i + 3] in b" \t\r\n"):
            j = data.find(b"\n", i)
            i = n if j < 0 else j + 1
            continue
        if data.startswith(b"/*", i):
            j = data.find(b"*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == b";":
            s = bytes(cur).strip()
            if s:
                out.append(s)
            cur = bytearray()
            i += 1
            continue
        cur += c
        i += 1
    s = bytes(cur).strip()
    if s:
        out.append(s)
    return out


ESCAPES = {ord("0"): b"\0", ord("n"): b"\n", ord("r"): b"\r", ord("t"): b"\t",
           ord("b"): b"\b", ord("Z"): b"\x1a"}


def parse_values(s: bytes, i: int):
    """Parse '(v, v, ...), (...)' starting at s[i]. Yields lists of python values."""
    n = len(s)
    rows = []
    while i < n:
        while i < n and s[i] in b" \t\r\n,":
            i += 1
        if i >= n or s[i] != ord("("):
            break
        i += 1
        row = []
        while True:
            while s[i] in b" \t\r\n":
                i += 1
            ch = s[i]
            if ch == ord("'"):
                i += 1
                buf = bytearray()
                while True:
                    d = s[i]
                    if d == ord("\\"):
                        e = s[i + 1]
                        buf += ESCAPES.get(e, bytes([e]))
                        i += 2
                    elif d == ord("'"):
                        if i + 1 < n and s[i + 1] == ord("'"):
                            buf += b"'"
                            i += 2
                        else:
                            i += 1
                            break
                    else:
                        buf.append(d)
                        i += 1
                row.append(bytes(buf))
            else:
                j = i
                while s[j] not in b",)":
                    j += 1
                tok = s[i:j].strip().decode()
                i = j
                if tok.upper() == "NULL":
                    row.append(None)
                elif tok.lower().startswith("0x"):
                    row.append(bytes.fromhex(tok[2:]))
                elif tok.upper().startswith("X'"):
                    row.append(bytes.fromhex(tok[2:-1]))
                elif re.fullmatch(r"-?\d+", tok):
                    row.append(int(tok))
                else:
                    row.append(float(tok))
            while s[i] in b" \t\r\n":
                i += 1
            if s[i] == ord(","):
                i += 1
                continue
            if s[i] == ord(")"):
                i += 1
                break
            raise ValueError("bad VALUES near %r" % s[i:i + 40])
        rows.append(row)
    return rows


# ------------------------------------------------------------------ DDL

INT_TYPES = ("tinyint", "smallint", "mediumint", "int", "integer", "bigint", "bit", "year")
REAL_TYPES = ("float", "double", "decimal", "real", "numeric")
BLOB_TYPES = ("blob", "tinyblob", "mediumblob", "longblob", "binary", "varbinary")
DATE_TYPES = ("datetime", "timestamp", "date", "time")


def split_top(s: str, sep=","):
    parts, depth, cur, q = [], 0, "", None
    for ch in s:
        if q:
            cur += ch
            if ch == q:
                q = None
            continue
        if ch in "'`\"":
            q = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def key_cols(spec: str):
    cols = []
    for c in split_top(spec):
        c = re.sub(r"\(\d+\)", "", c).strip()
        c = re.sub(r"\s+(ASC|DESC)$", "", c, flags=re.I)
        cols.append('"%s"' % c.strip("`"))
    return ", ".join(cols)


class Table:
    def __init__(self, name):
        self.name = name
        self.cols = []        # (name, sqltype, defn)
        self.coltypes = {}
        self.enums = []       # (col, values, is_set)
        self.pk = None
        self.uniques = []
        self.indexes = []
        self.autoinc = None
        self.autoinc_start = None


def parse_create(stmt: str) -> Table:
    m = re.match(r"CREATE TABLE\s+`?(\w+)`?\s*\((.*)\)\s*(.*)$", stmt, re.S | re.I)
    t = Table(m.group(1))
    body, opts = m.group(2), m.group(3)
    am = re.search(r"AUTO_INCREMENT=(\d+)", opts, re.I)
    if am:
        t.autoinc_start = int(am.group(1))
    for part in split_top(body):
        up = part.upper()
        if up.startswith("PRIMARY KEY"):
            t.pk = key_cols(re.search(r"\((.*)\)", part, re.S).group(1))
            continue
        if up.startswith("UNIQUE"):
            t.uniques.append(key_cols(re.search(r"\((.*)\)", part, re.S).group(1)))
            continue
        if up.startswith(("KEY", "INDEX")):
            km = re.match(r"(?:KEY|INDEX)\s+`?(\w+)`?\s*\((.*)\)", part, re.S | re.I)
            t.indexes.append((km.group(1), key_cols(km.group(2))))
            continue
        if up.startswith(("FULLTEXT", "SPATIAL", "CONSTRAINT", "FOREIGN")):
            continue
        cm = re.match(r"`(\w+)`\s+(\w+)(\s*\((?:[^()']|'(?:[^'\\]|\\.|'')*')*\))?(.*)$", part, re.S)
        name, typ, targs, rest = cm.group(1), cm.group(2).lower(), cm.group(3) or "", cm.group(4)
        if typ in ("enum", "set"):
            vals = re.findall(r"'((?:[^'\\]|\\.|'')*)'", targs)
            t.enums.append((name, ",".join(vals), 1 if typ == "set" else 0))
        if typ in INT_TYPES:
            st = "INTEGER"
        elif typ in REAL_TYPES:
            st = "REAL"
        elif typ in BLOB_TYPES:
            st = "BLOB"
        else:
            st = "TEXT"
        rest = re.sub(r"\b(unsigned|zerofill|signed)\b", "", rest, flags=re.I)
        rest = re.sub(r"\bCHARACTER SET\s+\w+|\bCOLLATE\s+\w+", "", rest, flags=re.I)
        rest = re.sub(r"\bON UPDATE\s+current_timestamp(\(\))?", "", rest, flags=re.I)
        rest = re.sub(r"\bCOMMENT\s+'(?:[^'\\]|\\.|'')*'", "", rest, flags=re.I)
        rest = re.sub(r"\bcurrent_timestamp\(\)", "CURRENT_TIMESTAMP", rest, flags=re.I)
        auto = bool(re.search(r"\bAUTO_INCREMENT\b", rest, re.I))
        rest = re.sub(r"\bAUTO_INCREMENT\b", "", rest, flags=re.I)
        notnull = bool(re.search(r"\bNOT NULL\b", rest, re.I))
        has_default = bool(re.search(r"\bDEFAULT\b", rest, re.I))
        if notnull and not has_default and not auto:
            # MySQL (non-strict) fills implicit defaults for omitted NOT NULL columns.
            if st in ("INTEGER", "REAL"):
                rest += " DEFAULT 0"
            elif st == "BLOB":
                rest += " DEFAULT X''"
            elif typ in DATE_TYPES:
                rest += " DEFAULT '0000-00-00 00:00:00'"
            elif typ == "enum":
                first = re.findall(r"'((?:[^'\\]|\\.|'')*)'", targs)
                rest += " DEFAULT '%s'" % (first[0] if first else "")
            else:
                rest += " DEFAULT ''"
        collate = " COLLATE NOCASE" if st == "TEXT" and typ not in DATE_TYPES else ""
        t.coltypes[name] = st
        t.cols.append([name, st, " ".join(rest.split()) + collate, auto])
    for c in t.cols:
        if c[3]:
            t.autoinc = c[0]
    return t


def table_sql(t: Table):
    lines = []
    pk_is_auto = t.autoinc and t.pk == '"%s"' % t.autoinc
    for name, st, rest, auto in t.cols:
        if auto and pk_is_auto:
            rest = re.sub(r"\bNOT NULL\b", "", rest, flags=re.I)
            lines.append('"%s" INTEGER PRIMARY KEY AUTOINCREMENT %s' % (name, rest.replace("COLLATE NOCASE", "")))
        else:
            lines.append('"%s" %s %s' % (name, st, rest))
    if t.pk and not pk_is_auto:
        lines.append("PRIMARY KEY (%s)" % t.pk)
    for u in t.uniques:
        lines.append("UNIQUE (%s)" % u)
    out = ['CREATE TABLE "%s" (\n  %s\n)' % (t.name, ",\n  ".join(lines))]
    for iname, cols in t.indexes:
        out.append('CREATE INDEX "%s__%s" ON "%s" (%s)' % (t.name, iname, t.name, cols))
    return out


def trigger_sql(t: Table):
    out = []
    for col, vals, is_set in t.enums:
        lit = vals.replace("'", "''")
        cond = "typeof(NEW.\"{c}\")='integer' OR NEW.\"{c}\" GLOB '[0-9]*'".format(c=col)
        upd = "UPDATE \"{t}\" SET \"{c}\"=m2_enum_text(NEW.\"{c}\", '{v}', {s}) WHERE rowid=NEW.rowid;".format(
            t=t.name, c=col, v=lit, s=is_set)
        out.append('CREATE TRIGGER "%s__%s_ai" AFTER INSERT ON "%s" WHEN %s BEGIN %s END' % (t.name, col, t.name, cond, upd))
        out.append('CREATE TRIGGER "%s__%s_au" AFTER UPDATE OF "%s" ON "%s" WHEN %s BEGIN %s END' % (t.name, col, col, t.name, cond, upd))
    return out


# ------------------------------------------------------------------ python impls of shim functions

def enum_text(v, vals, is_set):
    try:
        n = int(v)
    except (TypeError, ValueError):
        return v
    values = vals.split(",")
    if not is_set:
        return values[n - 1] if 1 <= n <= len(values) else ""
    return ",".join(x for i, x in enumerate(values) if n & (1 << i))


def to_sql_value(v, coltype, enumdef):
    if isinstance(v, bytes):
        if coltype == "BLOB" or b"\0" in v:
            return v, "?"
        try:
            v = v.decode("utf-8")
        except UnicodeDecodeError:
            return v, "CAST(? AS TEXT)"
        if enumdef is not None and coltype == "TEXT":
            v = enum_text(v, *enumdef)
        return v, "?"
    if enumdef is not None and isinstance(v, int):
        return enum_text(v, *enumdef), "?"
    return v, "?"


def convert(sql_path: str, db_path: str, test_account):
    data = open(sql_path, "rb").read()
    if os.path.exists(db_path):
        os.remove(db_path)
    con = sqlite3.connect(db_path)
    con.create_function("m2_enum_text", 3, enum_text, deterministic=True)
    con.execute("PRAGMA journal_mode=OFF")
    con.execute("PRAGMA synchronous=OFF")
    con.execute("CREATE TABLE _m2_enum (tbl TEXT, col TEXT, vals TEXT, is_set INTEGER)")
    tables = {}
    for raw in split_statements(data):
        head = raw[:64].upper()
        if head.startswith(b"CREATE TABLE"):
            t = parse_create(raw.decode("utf-8", "surrogateescape"))
            tables[t.name] = t
            for s in table_sql(t):
                con.execute(s)
            for col, vals, is_set in t.enums:
                con.execute("INSERT INTO _m2_enum VALUES (?,?,?,?)", (t.name, col, vals, is_set))
            if t.autoinc and t.autoinc_start and t.autoinc_start > 1:
                con.execute("INSERT INTO sqlite_sequence(name, seq) VALUES (?, ?)", (t.name, t.autoinc_start - 1))
        elif head.startswith(b"INSERT INTO"):
            m = re.match(rb"INSERT INTO\s+`(\w+)`\s*(?:\(([^)]*)\))?\s*VALUES\s*", raw, re.S)
            name = m.group(1).decode()
            t = tables[name]
            cols = [c.strip().strip("`") for c in m.group(2).decode().split(",")] if m.group(2) else [c[0] for c in t.cols]
            enums = {c: (v, s) for c, v, s in t.enums}
            for row in parse_values(raw, m.end()):
                vals, marks = [], []
                for c, v in zip(cols, row):
                    sv, mark = to_sql_value(v, t.coltypes[c], enums.get(c))
                    vals.append(sv)
                    marks.append(mark)
                con.execute('INSERT INTO "%s" (%s) VALUES (%s)' % (name, ", ".join('"%s"' % c for c in cols), ", ".join(marks)), vals)
        elif head.startswith((b"DROP TABLE", b"LOCK TABLES", b"UNLOCK TABLES", b"SET ", b"CREATE DATABASE", b"USE ")):
            continue
        elif head.startswith((b"CREATE ALGORITHM", b"CREATE VIEW", b"CREATE TRIGGER", b"CREATE PROCEDURE", b"CREATE FUNCTION")):
            print("  skip: %s" % raw[:80].decode(errors="replace"), file=sys.stderr)
        else:
            print("  skip: %s" % raw[:80].decode(errors="replace"), file=sys.stderr)
    for t in tables.values():
        for s in trigger_sql(t):
            con.execute(s)
    if test_account and "account" in tables:
        login, pw = test_account
        cur = con.execute("UPDATE account SET password=? WHERE login=?", (mysql_password(pw), login))
        if cur.rowcount == 0:
            con.execute("INSERT INTO account (login, password, social_id, status) VALUES (?, ?, '1234567', 'OK')",
                        (login, mysql_password(pw)))
    con.commit()
    con.execute("PRAGMA journal_mode=WAL")
    con.execute("VACUUM")
    con.close()
    return tables


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sql_dir")
    ap.add_argument("out_dir")
    ap.add_argument("--test-account", default=None, help="LOGIN:PASSWORD to (re)seed in account.account")
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)
    test = tuple(a.test_account.split(":", 1)) if a.test_account else None
    for name in DATABASES:
        src = os.path.join(a.sql_dir, name + ".sql")
        dst = os.path.join(a.out_dir, name + ".sqlite3")
        tables = convert(src, dst, test if name == "account" else None)
        print("%-8s %3d tables -> %s" % (name, len(tables), dst))
    # db opens SQL_HOTBACKUP at boot; upstream ships no schema for it.
    hb = os.path.join(a.out_dir, "hotbackup.sqlite3")
    if os.path.exists(hb):
        os.remove(hb)
    con = sqlite3.connect(hb)
    con.execute("CREATE TABLE _m2_enum (tbl TEXT, col TEXT, vals TEXT, is_set INTEGER)")
    con.execute("PRAGMA journal_mode=WAL")
    con.close()


if __name__ == "__main__":
    main()
