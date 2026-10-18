#!/usr/bin/env python3
"""Run a DW1_SMOKE battery to the end, restarting past crashes and hangs.

usage: tools/smoke.py --bin build/linux32/dw1 --cue DISC.cue --replay continue.txt
                      [--card card1.mcd] [--battery maps] [--first 0] [--last 254]
                      [--frames 300] [--stall 20] [--workdir smoke-run]
                      [--jobs N] [--chunk 8]

The replay must take the game from boot into gameplay (a Continue of the card);
the battery starts when it ends. Each run is offscreen and in turbo. A run that
crashes or logs no new "smoke:" line for --stall seconds is stopped; the item it
was on is reported and the next run starts after it. Per item, the report lists
crashes (with the call stack resolved by addr2line), hangs, divisions by zero
and unsupported GPU commands logged while it was current.

With --jobs N (default: half the CPUs) the range is cut into chunks of --chunk
items that N workers take in turn, each in its own folder under --workdir with
its own copy of the card; the reports are merged at the end.
"""
import argparse
import concurrent.futures
import threading
import os
import re
import shutil
import subprocess
import sys
import time

ITEM_RE = re.compile(r"dw1: smoke: (map|model|battle) (\d+)(.*)")
PRINT_LOCK = threading.Lock()


def resolve(exe, log_lines):
    """addr2line the dw1(+0x...) frames of a crash stack"""
    addrs = [m.group(1) for l in log_lines for m in [re.search(r"dw1\(\+(0x[0-9a-f]+)\)", l)] if m]
    if not addrs:
        return []
    out = subprocess.run(["addr2line", "-f", "-i", "-e", exe] + addrs, capture_output=True, text=True).stdout.split("\n")
    frames = []
    for i in range(0, len(out) - 1, 2):
        func, where = out[i], out[i + 1]
        if func in ("on_crash", "on_fpe", "??"):
            continue
        frames.append("%s %s" % (func, re.sub(r".*/dw1-port/", "", where)))
    return frames[:8]


def run_once(args, first, last, workdir, env_base):
    os.makedirs(workdir, exist_ok=True)
    for c in ("card1.mcd", "card2.mcd"):
        p = os.path.join(workdir, c)
        if os.path.exists(p):
            os.remove(p)
    if args.card:
        shutil.copy(args.card, os.path.join(workdir, "card1.mcd"))
    log_path = os.path.join(workdir, "run-%03d.log" % first)
    env = dict(env_base)
    env.update({
        "SDL_VIDEO_DRIVER": "offscreen", "SDL_AUDIO_DRIVER": "dummy", "DW1_TURBO": "1",
        "DW1_REPLAY": os.path.abspath(args.replay),
        "DW1_SMOKE": "%s:%d-%d" % (args.battery, first, last),
        "DW1_SMOKE_FRAMES": str(args.frames),
    })
    with open(log_path, "w") as log:
        proc = subprocess.Popen([os.path.abspath(args.bin), os.path.abspath(args.cue)],
                                cwd=workdir, stdout=log, stderr=subprocess.STDOUT, env=env)
        last_size, last_change, started = 0, time.time(), time.time()
        while proc.poll() is None:
            time.sleep(0.5)
            text = open(log_path, errors="replace").read()
            items = ITEM_RE.findall(text)
            size = len(items)
            if size != last_size:
                last_size, last_change = size, time.time()
            # the replay itself may take a while before the first item
            limit = args.stall if size else args.stall + 120
            if time.time() - last_change > limit:
                proc.kill()
                proc.wait()
                return log_path, "hang"
    return log_path, ("done" if "dw1: smoke: done" in open(log_path, errors="replace").read() else "crash")


def run_range(args, first, last, workdir):
    """run items first..last, restarting after crashes and hangs; returns the report"""
    report = {}  # id -> dict(name, notes)
    while first <= last:
        log_path, how = run_once(args, first, last, workdir, os.environ)
        lines = open(log_path, errors="replace").read().splitlines()
        cur = None
        for l in lines:
            m = ITEM_RE.match(l)
            if m:
                cur = int(m.group(2))
                rest = m.group(3).strip()
                if rest.startswith("still busy"):
                    report.setdefault(cur, {"name": "", "notes": []})["notes"].append("busy (event did not end)")
                else:
                    report.setdefault(cur, {"name": rest, "notes": []})
                continue
            if cur is None:
                continue
            if "division by zero" in l:
                report[cur]["notes"].append("division by zero " + (resolve(args.bin, [l]) or ["?"])[0])
            elif "unsupported command" in l:
                report[cur]["notes"].append("unsupported GPU command")
        if how == "done":
            break
        if cur is None:
            with PRINT_LOCK:
                print("no item started in %d-%d (replay failed?), see %s" % (first, last, log_path), flush=True)
            break
        if how == "crash":
            stack = resolve(args.bin, lines[next((i for i, l in enumerate(lines) if "crashed" in l), len(lines)):])
            report[cur]["notes"].append("CRASH: " + " <- ".join(stack[:4]))
        else:
            report[cur]["notes"].append("HANG (no progress for %ds)" % args.stall)
        with PRINT_LOCK:
            print("%s at %s %d, restarting after it" % (how, args.battery, cur), flush=True)
        first = cur + 1
    return report


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--cue", required=True)
    ap.add_argument("--replay", required=True)
    ap.add_argument("--card")
    ap.add_argument("--battery", default="maps")
    ap.add_argument("--first", type=int, default=0)
    ap.add_argument("--last", type=int, default=254)
    ap.add_argument("--frames", type=int, default=300)
    ap.add_argument("--stall", type=int, default=20)
    ap.add_argument("--workdir", default="smoke-run")
    ap.add_argument("--jobs", type=int, default=0)
    ap.add_argument("--chunk", type=int, default=8)
    args = ap.parse_args()

    jobs = args.jobs or max(1, (os.cpu_count() or 2) // 2)
    chunks = [(a, min(a + args.chunk - 1, args.last)) for a in range(args.first, args.last + 1, args.chunk)]
    report = {}
    print("%s battery: items %d-%d in %d chunks, %d parallel jobs" % (args.battery, args.first, args.last, len(chunks), jobs), flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(run_range, args, a, b, os.path.join(args.workdir, "chunk-%03d" % a)) for a, b in chunks]
        for f in concurrent.futures.as_completed(futures):
            report.update(f.result())

    bad = 0
    print("\n== %s battery: %d items run" % (args.battery, len(report)))
    for k in sorted(report):
        notes = report[k]["notes"]
        if not notes:
            continue
        bad += 1
        uniq = []
        for n in notes:
            if n not in uniq:
                uniq.append(n)
        print("%3d %-10s %s" % (k, report[k]["name"], "; ".join(uniq)))
    print("%d with problems, %d clean" % (bad, len(report) - bad))
    return 0


sys.exit(main())
