"""Turns the compiler log into short messages for the tutor's tiny screen."""
import re
import sys

log = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
out = []
for line in log:
    m = re.search(r"user_code\.ino:(\d+):(?:\d+:)?\s*(?:fatal )?error:\s*(.*)", line)
    if m:
        msg = m.group(2).replace("user_setup", "setup").replace("user_loop", "loop")
        out.append(f"L{m.group(1)}: {msg}")
    elif "undefined reference to" in line:
        name = re.search(r"undefined reference to [`'](.*?)'", line)
        out.append(f"Unknown function: {name.group(1) if name else '?'}")
    if len(out) >= 3:
        break

if not out:
    out.append("Build failed. Check the Actions tab on GitHub.")
for o in out:
    # The tutor screen only has plain ASCII characters
    for a, b in (("‘", "'"), ("’", "'"), ("“", '"'), ("”", '"')):
        o = o.replace(a, b)
    o = o.encode("ascii", "replace").decode()
    print(o[:120])
