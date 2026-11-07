"""Test run without taking focus: log + periodic window screenshots in D:/CodexBuilds/doritos/runs/<id>/.

Usage: run_test.py [--seconds 60] [--shot-every 10] [--cdb] [--args ...] [--env K=V ...]
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import time
from datetime import datetime
from pathlib import Path

CDB = r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
CDB_COMMANDS = ['sxe -c ".echo ===AV===;r;kn 25;gn" av', 'sxd -c "gn" ch', 'sxi ld', 'g',
                '.echo ===EXIT===', 'kn 30', 'q']
DATA = Path(r'D:\CodexBuilds\doritos')
BUILD = DATA / 'build' / 'win-amd64-release'
EXE_NAME = 'doritoscrashcourse.exe'

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--seconds', type=float, default=60)
ap.add_argument('--shot-every', type=float, default=10)
ap.add_argument('--env', nargs='*', default=[])
ap.add_argument('--args', nargs='*', default=[])
ap.add_argument('--input', default='')
ap.add_argument('--nav', action='store_true',
                help='to gameplay: press A every 6 s from 35 s; B when the "not signed in to LIVE" dialog shows')
ap.add_argument('--peek', nargs=2, action='append', default=[], metavar=('FILE', 'SECONDS'),
                help='debug_peek request file handed to the game at SECONDS (repeatable)')
ap.add_argument('--cdb', action='store_true')
ap.add_argument('--nav-start', type=float, default=35, help='seconds before --nav starts pressing A (raise on a loaded machine)')
opts = ap.parse_args()

run = DATA / 'runs' / datetime.now().strftime('%Y%m%d-%H%M%S')
run.mkdir(parents=True)
env = dict(os.environ)
for kv in opts.env:
    k, v = kv.split('=', 1)
    env[k] = v
if opts.nav and not opts.input:
    (run / 'input.txt').write_text('# --nav\n')
    env['DORITOS_INPUT'] = str(run / 'input.txt')
if opts.input:
    import shutil
    shutil.copy(opts.input, run / 'input.txt')
    env['DORITOS_INPUT'] = str(run / 'input.txt')
env['DORITOS_PEEK'] = str(run)
log = run / 'game.log'
args = [str(BUILD / EXE_NAME), f'--game_data_root={DATA / "game_files"}', f'--user_data_root={DATA / "userdata"}',
        f'--log_file={log}', '--log_flush_interval=1', '--no-fullscreen'] + ([] if any(a.startswith('--log_level') for a in opts.args) else ['--log_level=info']) + opts.args
if opts.cdb:
    (run / 'cmds.txt').write_text('\n'.join(CDB_COMMANDS) + '\n')
    args = [CDB, '-lines', '-y', str(BUILD), '-cf', str(run / 'cmds.txt')] + args

user32 = ctypes.windll.user32
user32.SetProcessDPIAware()


def find_window(pid_set):
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if user32.IsWindowVisible(hwnd) and pid.value in pid_set:
            found.append(hwnd)
        return True
    user32.EnumWindows(cb, 0)
    return found[0] if found else None


def game_pids():
    out = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {EXE_NAME}', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    return {int(l.split('","')[1]) for l in out.splitlines() if l.startswith('"')}


def grab():
    """Window contents via PrintWindow(PW_RENDERFULLCONTENT): works while other windows cover it."""
    from PIL import Image
    hwnd = find_window(game_pids())
    if not hwnd:
        return None
    r = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    w, h = r.right, r.bottom
    if w < 50 or h < 50:
        return None
    gdi32 = ctypes.windll.gdi32
    hdc = user32.GetDC(hwnd)
    mdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mdc, bmp)
    user32.PrintWindow(hwnd, mdc, 3)  # PW_CLIENTONLY | PW_RENDERFULLCONTENT

    class BIH(ctypes.Structure):
        _fields_ = [('biSize', wt.DWORD), ('biWidth', wt.LONG), ('biHeight', wt.LONG), ('biPlanes', wt.WORD),
                    ('biBitCount', wt.WORD), ('biCompression', wt.DWORD), ('biSizeImage', wt.DWORD),
                    ('biXPelsPerMeter', wt.LONG), ('biYPelsPerMeter', wt.LONG), ('biClrUsed', wt.DWORD),
                    ('biClrImportant', wt.DWORD)]
    bih = BIH(ctypes.sizeof(BIH), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bih), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mdc)
    user32.ReleaseDC(hwnd, hdc)
    return Image.frombuffer('RGBA', (w, h), buf, 'raw', 'BGRA', 0, 1).convert('RGB')


def signin_dialog(im):
    # Full-width blue band at mid height over a dimmed (near-black) screen.
    w, h = im.size
    near = lambda c, ref: all(abs(a - b) < 30 for a, b in zip(c, ref))
    band = [im.getpixel((int(w * x), int(h * 0.5))) for x in (0.05, 0.1, 0.9, 0.95)]
    if not (all(near(c, (35, 103, 169)) for c in band) and max(im.getpixel((int(w * 0.5), int(h * 0.2)))) < 40):
        return False
    # The "Saving..." box looks the same; the sign-in warning has three long lines of white text.
    text = sum(1 for y in range(int(h * .44), int(h * .58), 2) for x in range(int(w * .15), int(w * .85), 2)
               if min(im.getpixel((x, y))) > 180)
    return text > 800


def press(button, at, hold=500):
    with open(run / 'input.txt', 'a') as f:
        f.write(f'{at:.1f} {button} {hold}\n')


def shot(name):
    im = grab()
    if im is not None:
        im.save(run / name)


si = subprocess.STARTUPINFO()
si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
si.wShowWindow = 4  # SW_SHOWNOACTIVATE
t0 = time.monotonic()
next_shot = opts.shot_every
nav_last_a, nav_done, nav_seen = 0.0, not opts.nav, 0
with (run / ('cdb.txt' if opts.cdb else 'stdio.log')).open('w') as out:
    p = subprocess.Popen(args, cwd=BUILD, env=env, startupinfo=si, stdout=out, stderr=subprocess.STDOUT)
    while p.poll() is None and time.monotonic() - t0 < opts.seconds:
        time.sleep(0.5)
        el = time.monotonic() - t0
        if not nav_done and el >= opts.nav_start:
            im = grab()
            nav_seen = nav_seen + 1 if im is not None and signin_dialog(im) else 0
            if nav_seen >= 3:  # 1.5 s of the dialog, not a flash
                press('B', el + 0.5)
                # level start: run forward and jump now and then
                press('LY=1', el + 15, 40000)
                for k in range(8):
                    press('A', el + 18 + 4 * k, 300)
                nav_done = True
                print(f'nav: dialog at {el:.0f} s -> B')
            elif el - nav_last_a >= 6:
                press('A', el + 0.5)
                nav_last_a = el
        for pk in list(opts.peek):
            if el >= float(pk[1]):
                import shutil as _sh
                _sh.copy(pk[0], run / 'peek.txt')
                opts.peek.remove(pk)
        if opts.shot_every and el >= next_shot:
            shot(f'shot_{int(el):04d}.png')
            next_shot += opts.shot_every
    exited = p.poll() is not None
    if not exited:
        subprocess.run(['taskkill', '/IM', EXE_NAME, '/T', '/F'], capture_output=True)
        subprocess.run(['taskkill', '/PID', str(p.pid), '/T', '/F'], capture_output=True)
    p.wait()
print(f'run={run} exited={exited} code={p.returncode} secs={time.monotonic() - t0:.0f}')
