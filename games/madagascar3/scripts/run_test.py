"""Hidden-window test run: scripted pad, periodic frame dumps, log in runs/<timestamp>/.

Usage: run_test.py [--seconds 60] [--input inputs/x.txt] [--dump-every 300] [--env K=V ...] [--cdb]
The input file (project/src/script_input.h format) is copied to runs/<id>/input.txt; lines can be
appended there while the game runs. Frame dumps are moved into the run dir as PNG.
--cdb runs under cdb and writes the stack of every access violation to runs/<id>/cdb.txt.
"""
import argparse
import os
import shutil
import subprocess
import time
from datetime import datetime
from pathlib import Path

CDB = r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
CDB_COMMANDS = ['sxe -c ".echo ===AV===;r;kn 25;gn" av', 'sxd -c "gn" ch', 'sxi ld', 'g',
                '.echo ===EXIT===', 'kn 30', 'q']

root = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--seconds', type=float, default=60)
ap.add_argument('--input', default='')
ap.add_argument('--dump-every', type=int, default=0)
ap.add_argument('--env', nargs='*', default=[])
ap.add_argument('--args', nargs='*', default=[])
ap.add_argument('--cdb', action='store_true')
opts = ap.parse_args()

build = Path((root / 'build-location.txt').read_text(encoding='utf-8-sig').strip())
exe = build / 'madagascar3.exe'
run = root / 'runs' / datetime.now().strftime('%Y%m%d-%H%M%S')
run.mkdir(parents=True)
env = dict(os.environ)
if opts.input:
    shutil.copy(root / opts.input, run / 'input.txt')
    env['MADAGASCAR3_INPUT'] = str(run / 'input.txt')
if opts.dump_every:
    env['MADAGASCAR3_DUMP_EVERY'] = str(opts.dump_every)
for kv in opts.env:
    k, v = kv.split('=', 1)
    env[k] = v
for old in build.glob('frame_*.bmp'):
    old.unlink()
log = run / 'game.log'
args = [str(exe), f'--game_data_root={root / "game_files"}', f'--user_data_root={root / "runs/userdata"}',
        f'--log_file={log}', '--log_level=info', '--log_flush_interval=1', '--no-fullscreen'] + opts.args
if opts.cdb:
    (run / 'cmds.txt').write_text('\n'.join(CDB_COMMANDS) + '\n')
    args = [CDB, '-lines', '-y', str(build), '-cf', str(run / 'cmds.txt')] + args
si = subprocess.STARTUPINFO()
si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
si.wShowWindow = 4  # SW_SHOWNOACTIVATE: visible but never takes focus
t0 = time.monotonic()
with (run / ('cdb.txt' if opts.cdb else 'stdio.log')).open('w') as out:
    p = subprocess.Popen(args, cwd=build, env=env, startupinfo=si, stdout=out, stderr=subprocess.STDOUT)
    while p.poll() is None and time.monotonic() - t0 < opts.seconds:
        time.sleep(0.5)
    exited = p.poll() is not None
    if not exited:
        subprocess.run(['taskkill', '/IM', 'madagascar3.exe', '/T', '/F'], capture_output=True)
        subprocess.run(['taskkill', '/PID', str(p.pid), '/T', '/F'], capture_output=True)
    p.wait()
try:
    from PIL import Image
except ImportError:
    Image = None
for bmp in sorted(build.glob('frame_*.bmp')):
    if Image:
        Image.open(bmp).save(run / (bmp.stem + '.png'))
        bmp.unlink()
    else:
        shutil.move(str(bmp), run / bmp.name)
print(f'run={run} exited={exited} code={p.returncode} secs={time.monotonic() - t0:.0f}')
