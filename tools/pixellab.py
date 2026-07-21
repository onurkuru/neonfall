#!/usr/bin/env python3
"""Talk to PixelLab's MCP server over HTTP.

The server answers JSON-RPC with server-sent events, so responses arrive as
`data:` lines rather than a plain JSON body. Transport is curl because this
machine's Python has no usable CA bundle.

  pixellab.py list
  pixellab.py call <tool> '<json args>'
"""
import json, os, subprocess, sys

URL = "https://api.pixellab.ai/mcp"


def rpc(method, params=None, req_id=1):
    token = os.environ.get("PIXELLAB_SECRET", "")
    body = {"jsonrpc": "2.0", "id": req_id, "method": method}
    if params is not None:
        body["params"] = params
    out = subprocess.run(
        ["curl", "-s", "-X", "POST", URL,
         "-H", f"Authorization: Bearer {token}",
         "-H", "Content-Type: application/json",
         "-H", "Accept: application/json, text/event-stream",
         "-d", json.dumps(body)],
        capture_output=True).stdout.decode("utf-8", "replace")

    for line in out.splitlines():
        if line.startswith("data: "):
            return json.loads(line[6:])
    raise RuntimeError(f"no data frame in response:\n{out[:400]}")


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else "list"

    if what == "list":
        for t in rpc("tools/list")["result"]["tools"]:
            first = t["description"].splitlines()[0]
            print(f"{t['name']}\n    {first[:150]}")
    elif what == "schema":
        for t in rpc("tools/list")["result"]["tools"]:
            if t["name"] == sys.argv[2]:
                print(json.dumps(t["inputSchema"], indent=1)[:4000])
    elif what == "call":
        args = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        r = rpc("tools/call", {"name": sys.argv[2], "arguments": args})
        res = r.get("result", {})
        for block in res.get("content", []):
            print(block.get("text", ""))
        if "error" in r:
            print("error:", json.dumps(r["error"]))
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
