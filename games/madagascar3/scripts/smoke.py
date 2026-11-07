"""Run the built executable for at most 20 seconds; record process/log facts only."""
import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--seconds', type=float, default=20)
parser.add_argument('--keyboard', action='store_true', help='Enable host keyboard controller bindings')
opts = parser.parse_args()
if opts.seconds <= 0:
    parser.error('--seconds must be positive')
build = root / 'project/out/build/win-amd64-release'
location = root / 'build-location.txt'
if location.is_file():
    build = Path(location.read_text(encoding='utf-8-sig').strip())
binary = build / 'madagascar3.exe'
if not binary.is_file():
    raise SystemExit('Build madagascar3.exe first')
run = root / 'runs/smoke'
run.mkdir(parents=True, exist_ok=True)
log = run / 'runtime.log'
args = [str(binary), f'--game_data_root={root / "game_files"}',
        f'--user_data_root={run / "userdata"}', f'--log_file={log}',
        '--log_level=info', '--log_flush_interval=1', '--no-fullscreen']
if opts.keyboard:
    args.append('--mnk_mode=true')
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
started = time.monotonic()
with (run / 'stdio.log').open('w') as output:
    proc = subprocess.Popen(args, cwd=build, startupinfo=startup,
                            stdout=output, stderr=subprocess.STDOUT)
    reason = 'exited'
    while proc.poll() is None:
        size = sum(p.stat().st_size for p in run.rglob('*') if p.is_file())
        if time.monotonic() - started >= opts.seconds:
            reason = f'{opts.seconds:g}-second limit'
            break
        if size >= 32 * 1024 ** 2:
            reason = 'log limit'
            break
        time.sleep(0.5)
    if proc.poll() is None:
        subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'], capture_output=True)
    proc.wait()
report = dict(command=args, stopReason=reason, exitCode=proc.returncode,
              binarySha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
              seconds=round(time.monotonic() - started, 2),
              limitations=['Process/log smoke test only; no visual validation.',
                           'A timeout termination is not a successful game exit.'])
(root / 'analysis/runtime-smoke.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
