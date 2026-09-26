#!/usr/bin/env python3
"""Drive the built parsex-mcp binary with hostile-but-small stdin lines.

Each line below used to crash the server with a stack overflow (SIGSEGV),
taking every later request down with it:

1. A ~200 KB request whose params nest 100k levels deep: parsed fine, then
   copied recursively when the params were read.
2. A malformed line with a ~200 KB unterminated string after "id": the
   std::regex used for id recovery recursed once per character.

The server must answer the first with a JSON-RPC -32600 error carrying the
request's id, skip the second (no id can be recovered), and still answer the
well-formed request that follows. Usage: pipe_hostile_input.py <parsex-mcp>
"""
import json
import subprocess
import sys

DEPTH = 100_000
LONG = 200_000


def main() -> int:
    binary = sys.argv[1]
    nested = "[" * DEPTH + "]" * DEPTH
    lines = [
        '{"jsonrpc":"2.0","id":9,"method":"tools/call",'
        '"params":{"name":"parse_arxml","arguments":{"path":' + nested + "}}}",
        '{"jsonrpc":"2.0","id":"' + "A" * LONG,
        '{"jsonrpc":"2.0","id":10,"method":"tools/list"}',
    ]
    proc = subprocess.run(
        [binary],
        input="\n".join(lines) + "\n",
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )
    if proc.returncode != 0:
        print(f"FAIL: server exited with {proc.returncode}", file=sys.stderr)
        return 1
    responses = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    ids = [doc.get("id") for doc in responses]
    if ids != [9, 10]:
        print(f"FAIL: expected responses for ids [9, 10], got {ids}", file=sys.stderr)
        return 1
    error = responses[0].get("error", {})
    if error.get("code") != -32600 or "maximum depth" not in error.get("message", ""):
        print(f"FAIL: unexpected error for the deep request: {responses[0]}", file=sys.stderr)
        return 1
    if "result" not in responses[1]:
        print(f"FAIL: tools/list after hostile input did not succeed: {responses[1]}", file=sys.stderr)
        return 1
    print("server survived hostile input")
    return 0


if __name__ == "__main__":
    sys.exit(main())
