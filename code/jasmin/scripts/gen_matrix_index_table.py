#!/usr/bin/env python3
"""Generate or verify) the `gen_matrix_indexes` global for an avx2 kaiburr variant.

The matrix sampler builds A (k x k polynomials) 4-at-a-time with 4-way SHAKE128.
For each matrix entry it needs the 2-byte XOF domain separator (the (i,j) coordinate).
`gen_matrix_indexes` is those bytes for every entry, in flat row-major order, with the
non-transposed coordinates in the first half and the transposed ones in the second.

For flat index n, (i,j) = (n // k, n % k):
    half 1 (non-transposed, keygen, XOF(rho, j, i)) -> bytes [j, i]
    half 2 (transposed,     enc,    XOF(rho, i, j)) -> bytes [i, j]
Total = 4*k*k bytes.  The driver reads 4 entries (8 bytes) per `pos_entry = 8*i`,
with the transposed half at offset IDX_TABLE_SIZE/2 = 2*k*k.
"""
import argparse
import re
import sys


def table_bytes(k):
    """The full 4*k*k-byte table as a flat list of ints."""
    out = []
    for n in range(k * k): 
        i, j = divmod(n, k)
        out += [j, i]
    for n in range(k * k):
        i, j = divmod(n, k)
        out += [i, j]
    return out


def render_jinc(k):
    """Emit the .jinc literal, 4 entries (8 bytes) per line with (i,j) comments."""
    lines = ['require "../../common/avx2/gen_matrix_globals.jinc"',
             '',
             f'u8[{4 * k * k}] gen_matrix_indexes =',
             '{']
    entries = []
    for transposed in (False, True):
        for n in range(k * k):
            i, j = divmod(n, k)
            b0, b1 = (i, j) if transposed else (j, i)
            entries.append((b0, b1, i, j))

    rows = []
    for s in range(0, len(entries), 4):
        grp = entries[s:s + 4]
        data = ", ".join(f"0x{b0:02x}, 0x{b1:02x}" for (b0, b1, _, _) in grp)
        labs = " ".join(f"({i},{j})" for (_, _, i, j) in grp)
        rows.append((data, labs))
    half_rows = (k * k + 3) // 4
    for idx, (data, labs) in enumerate(rows):
        comma = "" if idx == len(rows) - 1 else ","
        lines.append(f"  {data}{comma} // {labs}")
        if idx == half_rows - 1:
            lines.append("")
    lines.append("};")
    return "\n".join(lines) + "\n"


def parse_table(text):
    body = text[text.index("{") + 1: text.rindex("}")]
    return [int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", body)]


def main():
    ap = argparse.ArgumentParser(description="generate/verify gen_matrix_indexes")
    ap.add_argument("k", type=int, help="module rank MLKEM_K")
    ap.add_argument("-o", "--output", help="write .jinc here instead of stdout")
    ap.add_argument("--verify", metavar="FILE",
                    help="compare an existing gen_matrix_globals.jinc against the computed table")
    args = ap.parse_args()

    if args.k % 2:
        print(f"warning: k={args.k} is odd; the full {4*args.k*args.k}-byte table is emitted, "
              f"but gen_matrix.jinc also needs the single-poly tail for entry "
              f"({args.k-1},{args.k-1}) (rc=0x{args.k-1:02x}{args.k-1:02x}).", file=sys.stderr)

    if args.verify:
        want = table_bytes(args.k)
        have = parse_table(open(args.verify).read())
        if want == have:
            print(f"OK: {args.verify} matches the computed k={args.k} table "
                  f"({len(want)} bytes).")
            return 0
        first = next((i for i in range(min(len(want), len(have))) if want[i] != have[i]), None)
        print(f"MISMATCH: {args.verify} has {len(have)} bytes, expected {len(want)}; "
              f"first differing byte: {first}.", file=sys.stderr)
        return 1

    out = render_jinc(args.k)
    if args.output:
        open(args.output, "w").write(out)
        print(f"wrote {args.output} ({4*args.k*args.k} bytes table)", file=sys.stderr)
    else:
        sys.stdout.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())