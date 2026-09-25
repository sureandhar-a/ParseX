#!/usr/bin/env python3
"""Measure ParseX token efficiency: raw ARXML in context vs MCP tool output.

Control vs treatment (after Twilio mcp-te-benchmark and Scalekit
mcp-vs-cli-benchmark): the control is pasting the whole .arxml file into
the model context; the treatment is calling one parsex-mcp tool and
reading only its response. The script drives the built
build/debug/parsex-mcp/parsex-mcp binary over stdio (newline-delimited
JSON-RPC, protocol version 2026-07-28) and counts tokens for both sides.

Token counting: tiktoken o200k_base when installed (exact, same family
used by the published XML/JSON density studies); otherwise a documented
chars/3.2 fallback calibrated on tests/fixtures/system-4.2.arxml
(71,059 B / 22,059 tok = 3.22 chars/tok). The counting method is always
printed and stored in the JSON artifact so results stay comparable.

Usage:
    python3 scripts/measure_tokens.py [--out FILE.json] [--mcp PATH]
    python3 scripts/measure_tokens.py --out /tmp/tokens.json

Output: markdown table on stdout + JSON artifact (inputs, byte counts,
token counts, method, git rev, binary version, date) for the evolving
log in docs/token-efficiency.md. Never hardcode prices here; convert
tokens to dollars at report time from the provider pricing page.
"""

import argparse
import datetime
import json
import pathlib
import subprocess
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_MCP = REPO_ROOT / "build" / "debug" / "parsex-mcp" / "parsex-mcp"
DEFAULT_CLI = REPO_ROOT / "build" / "debug" / "parsex-cli" / "parsex-cli"
PROTOCOL = "2026-07-28"

# Workload-anchored fixtures: the same files First run / testing guide use.
FIXTURES = [
    "tests/fixtures/schema_valid.arxml",
    "tests/fixtures/parsefile_complete.arxml",
    "tests/fixtures/tiny_valid.arxml",
    "tests/fixtures/system-4.2.arxml",
]
DIFF_PAIR = ("tests/fixtures/schema_valid.arxml", "tests/fixtures/system-4.2.arxml")


def make_counter():
    try:
        import tiktoken  # type: ignore

        enc = tiktoken.get_encoding("o200k_base")
        return (lambda s: len(enc.encode(s))), "tiktoken o200k_base (exact)"
    except ImportError:
        return (lambda s: max(1, len(s) // 3) if s else 0), "chars/3 fallback (no tiktoken)"


def mcp_call(mcp: pathlib.Path, request: dict) -> str:
    proc = subprocess.run(
        [str(mcp)],
        input=json.dumps(request) + "\n",
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"parsex-mcp exited {proc.returncode}: {proc.stderr[:500]}")
    return proc.stdout.strip()


def git_rev() -> str:
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            cwd=REPO_ROOT,
        )
        return out.stdout.strip() or "(unknown)"
    except OSError:
        return "(unknown)"


def cli_run(cli: pathlib.Path, args: list, cwd: pathlib.Path) -> tuple:
    """Run the CLI; return (stdout, stderr, returncode). Stdout only is
    measured: --json stays parseable and diagnostics go to stderr."""
    proc = subprocess.run(
        [str(cli)] + args, capture_output=True, text=True, cwd=cwd
    )
    return proc.stdout, proc.stderr, proc.returncode


def cli_rows(cli: pathlib.Path, count, entries: list) -> list:
    """Measure CLI surfaces for (label, path, cwd) entries. Failure rows
    (e.g. unsupported release) report the stderr cost, never zero."""
    rows = []
    for label, path, cwd in entries:
        raw = (pathlib.Path(cwd) / path).read_bytes() \
            if not pathlib.Path(path).is_absolute() else pathlib.Path(path).read_bytes()
        raw_tok = count(raw.decode("utf-8", errors="replace"))
        name = pathlib.Path(path).name
        commands = [
            (f"cli parse {name}", ["parse", "--input", str(path)]),
            (f"cli parse --json {name}", ["--json", "parse", "--input", str(path)]),
            (f"cli validate {name}", ["validate", "--input", str(path)]),
            (f"cli validate --json {name}",
             ["--json", "validate", "--input", str(path)]),
            (f"cli write dry-run {name}",
             ["write", "--input", str(path), "--output", "/tmp/parsex_measure_out.arxml"]),
            (f"cli write --json {name}",
             ["--json", "write", "--input", str(path), "--output",
              "/tmp/parsex_measure_out.arxml"]),
        ]
        _ = label
        for entry, cmd in commands:
            out, err, code = cli_run(cli, cmd, pathlib.Path(cwd))
            body, failed = (err, True) if code != 0 else (out, False)
            tok = count(body)
            rows.append({
                "entry": entry + (" (error, see stderr)" if failed else ""),
                "raw_bytes": len(raw),
                "raw_tokens": raw_tok,
                "tool_bytes": len(body.encode()),
                "tool_tokens": tok,
                "saving_pct": round((1 - tok / raw_tok) * 100, 1) if raw_tok else 0.0,
                "reduction_x": round(raw_tok / tok, 1) if tok else 0.0,
            })
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mcp", default=str(DEFAULT_MCP), help="parsex-mcp binary")
    parser.add_argument("--cli", default=str(DEFAULT_CLI), help="parsex-cli binary")
    parser.add_argument("--out", default="", help="write JSON artifact here")
    parser.add_argument("--cli-surfaces", action="store_true",
                        help="also measure CLI human/--json outputs for the fixtures")
    parser.add_argument("--real-dir", default="",
                        help="directory of third-party .arxml files to measure "
                             "(MCP + CLI); files stay out-of-tree, only aggregate "
                             "numbers enter the artifact")
    args = parser.parse_args()

    mcp = pathlib.Path(args.mcp)
    if not mcp.is_file():
        print(f"error: MCP binary not found: {mcp}", file=sys.stderr)
        print("build it first: cmake --preset default && cmake --build --preset default",
              file=sys.stderr)
        return 1

    count, method = make_counter()
    rows = []

    list_resp = mcp_call(
        mcp,
        {"jsonrpc": "2.0", "id": 1, "method": "tools/list",
         "_meta": {"io.modelcontextprotocol/protocolVersion": PROTOCOL}},
    )
    rows.append({"entry": "tools/list (one-time schema cost)",
                 "bytes": len(list_resp.encode()),
                 "tokens": count(list_resp)})

    for fixture in FIXTURES:
        raw = (REPO_ROOT / fixture).read_bytes()
        raw_text = raw.decode("utf-8", errors="replace")
        raw_tok = count(raw_text)
        for tool, tool_args in (
            ("parse_arxml", {"path": fixture}),
            ("validate_arxml", {"path": fixture}),
        ):
            resp = mcp_call(
                mcp,
                {"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                 "params": {"name": tool, "arguments": tool_args},
                 "_meta": {"io.modelcontextprotocol/protocolVersion": PROTOCOL}},
            )
            tok = count(resp)
            rows.append({
                "entry": f"{tool} {pathlib.Path(fixture).name}",
                "raw_bytes": len(raw),
                "raw_tokens": raw_tok,
                "tool_bytes": len(resp.encode()),
                "tool_tokens": tok,
                "saving_pct": round((1 - tok / raw_tok) * 100, 1) if raw_tok else 0.0,
                "reduction_x": round(raw_tok / tok, 1) if tok else 0.0,
            })

    base, target = DIFF_PAIR
    a = (REPO_ROOT / base).read_bytes().decode("utf-8", errors="replace")
    b = (REPO_ROOT / target).read_bytes().decode("utf-8", errors="replace")
    concat_tok = count(a + b)
    diff_resp = mcp_call(
        mcp,
        {"jsonrpc": "2.0", "id": 3, "method": "tools/call",
         "params": {"name": "diff_arxml",
                    "arguments": {"basePath": base, "targetPath": target}},
         "_meta": {"io.modelcontextprotocol/protocolVersion": PROTOCOL}},
    )
    diff_tok = count(diff_resp)
    rows.append({
        "entry": f"diff_arxml {pathlib.Path(base).name} -> {pathlib.Path(target).name}",
        "raw_bytes": len((a + b).encode()),
        "raw_tokens": concat_tok,
        "tool_bytes": len(diff_resp.encode()),
        "tool_tokens": diff_tok,
        "saving_pct": round((1 - diff_tok / concat_tok) * 100, 1),
        "reduction_x": round(concat_tok / diff_tok, 1),
    })

    if args.cli_surfaces:
        cli = pathlib.Path(args.cli)
        if not cli.is_file():
            print(f"error: CLI binary not found: {cli}", file=sys.stderr)
            return 1
        rows.extend(cli_rows(cli, count,
                             [(f, f, REPO_ROOT) for f in FIXTURES]))
        for label, cmd in (
            ("cli diff human", ["diff", "--base", base, "--target", target]),
            ("cli diff --json", ["--json", "diff", "--base", base, "--target", target]),
        ):
            out, _, _ = cli_run(cli, cmd, REPO_ROOT)
            tok = count(out)
            rows.append({
                "entry": f"{label} {pathlib.Path(base).name} -> "
                         f"{pathlib.Path(target).name}",
                "raw_bytes": len((a + b).encode()),
                "raw_tokens": concat_tok,
                "tool_bytes": len(out.encode()),
                "tool_tokens": tok,
                "saving_pct": round((1 - tok / concat_tok) * 100, 1),
                "reduction_x": round(concat_tok / tok, 1) if tok else 0.0,
            })

    if args.real_dir:
        real = pathlib.Path(args.real_dir)
        cli = pathlib.Path(args.cli)
        for child in sorted(real.glob("*.arxml")):
            raw = child.read_bytes()
            raw_tok = count(raw.decode("utf-8", errors="replace"))
            for tool, tool_args in (
                ("parse_arxml", {"path": str(child)}),
                ("validate_arxml", {"path": str(child)}),
            ):
                resp = mcp_call(
                    mcp,
                    {"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                     "params": {"name": tool, "arguments": tool_args},
                     "_meta": {"io.modelcontextprotocol/protocolVersion": PROTOCOL}},
                )
                tok = count(resp)
                rows.append({
                    "entry": f"{tool} third-party {child.name}",
                    "raw_bytes": len(raw),
                    "raw_tokens": raw_tok,
                    "tool_bytes": len(resp.encode()),
                    "tool_tokens": tok,
                    "saving_pct": round((1 - tok / raw_tok) * 100, 1) if raw_tok else 0.0,
                    "reduction_x": round(raw_tok / tok, 1) if tok else 0.0,
                })
            rows.extend(cli_rows(cli, count, [(str(child), str(child), real)]))

    artifact = {
        "date": datetime.date.today().isoformat(),
        "git_rev": git_rev(),
        "protocol": PROTOCOL,
        "counting_method": method,
        "entries": rows,
    }
    if args.out:
        pathlib.Path(args.out).write_text(json.dumps(artifact, indent=2) + "\n")
        print(f"# artifact: {args.out}", flush=True)

    print(f"# method: {method} | rev {artifact['git_rev']} | {artifact['date']}")
    print("| task | raw tok | tool tok | saved | reduction |")
    print("|---|---|---|---|---|")
    for row in rows:
        if "raw_tokens" not in row:
            print(f"| {row['entry']} | — | {row['tokens']} | — | — |")
        else:
            print(f"| {row['entry']} | {row['raw_tokens']:,} | "
                  f"{row['tool_tokens']:,} | {row['saving_pct']:.1f}% | "
                  f"{row['reduction_x']:.1f}x |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
