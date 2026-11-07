"""Live input for a running run_test.py session.

press.py BUTTON [hold_ms]            one press now
press.py --seq "A:3 START:2 LEFT:1"  presses in order, waiting the given seconds after each
Times are measured from the "script input:" log line (the driver's time base) of the newest
runs/<id> (or --run DIR)."""
import argparse
import re
import time
from datetime import datetime
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument('button', nargs='?')
ap.add_argument('hold', nargs='?', default='200')
ap.add_argument('--seq', default='')
ap.add_argument('--run')
a = ap.parse_args()
run = Path(a.run) if a.run else sorted((root / 'runs').glob('2026*'))[-1]


def start_time():
    for _ in range(120):
        log = run / 'game.log'
        if log.exists():
            for line in log.open(encoding='utf-8', errors='replace'):
                if 'script input:' in line:
                    m = re.match(r'\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+)\]', line)
                    return datetime.strptime(m.group(1), '%Y-%m-%d %H:%M:%S.%f')
        time.sleep(1)
    raise SystemExit('no script input line')


start = start_time()


def press(button, hold):
    t = (datetime.now() - start).total_seconds() + 0.3
    with (run / 'input.txt').open('a') as f:
        f.write(f'{t:.1f} {button} {hold}\n')
    print(f'{t:.1f} {button}', flush=True)


if a.seq:
    for item in a.seq.split():
        button, _, wait = item.partition(':')
        press(button, a.hold)
        time.sleep(float(wait or 1))
else:
    press(a.button, a.hold)
