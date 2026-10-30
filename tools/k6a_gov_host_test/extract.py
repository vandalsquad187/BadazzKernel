#!/usr/bin/env python3
"""Zieht die State-Machine-relevanten Teile aus k6a_gov.c nach extracted.h."""
import re, sys

src = open(sys.argv[1], encoding="utf-8").read()
margin = sys.argv[3] if len(sys.argv) > 3 else None   # optionaler Override fuer A/B

def block(start_re, open_ch="{", close_ch="}", end_semicolon=False):
    m = re.search(start_re, src, re.M)
    if not m:
        raise SystemExit("nicht gefunden: " + start_re)
    i = src.index(open_ch, m.start())
    depth = 0
    for j in range(i, len(src)):
        if src[j] == open_ch: depth += 1
        elif src[j] == close_ch:
            depth -= 1
            if depth == 0:
                end = j + 1
                if end_semicolon and src[end] == ";": end += 1
                return src[m.start():end]
    raise SystemExit("unbalanciert: " + start_re)

out = ["/* automatisch extrahiert aus k6a_gov.c, nicht von Hand aendern */"]
for name in ("K6A_HIST_N", "K6A_RAMP_UP_TICKS", "K6A_L4_MARGIN", "K6A_GOV_MAX_FREQS"):
    m = re.search(r"^#define\s+" + name + r"\b.*$", src, re.M)
    line = m.group(0)
    if name == "K6A_L4_MARGIN" and margin is not None:
        line = "#define K6A_L4_MARGIN %s" % margin
    out.append(line)
for pat in (r"^enum k6a_state\b.*$", r"^enum k6a_tsrc\b.*$", r"^enum k6a_profile\b.*$", r"^#define K6A_PROFILE_MAX\b.*$"):
    out.append(re.search(pat, src, re.M).group(0))
out.append(block(r"^struct k6a_hist_entry\s*\{", end_semicolon=True))
out.append(block(r"^struct k6a_gov\s*\{", end_semicolon=True))
out.append(block(r"^struct k6a_profile_def\s*\{", end_semicolon=True))
out.append(block(r"^static const struct k6a_profile_def profiles\[\]\s*=\s*\{", end_semicolon=True))
out.append("static struct k6a_gov *gov;")
for fn in ("k6a_hist_push", "set_state_locked", "ramp_next_cap", "state_machine", "battery_guard_tick"):
    out.append(block(r"^static \w[\w\s\*]*\b" + fn + r"\("))
open(sys.argv[2], "w").write("\n\n".join(out) + "\n")
print("extracted ->", sys.argv[2])
