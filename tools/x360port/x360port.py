#!/usr/bin/env python3
"""x360port: build a native PC port of any Xbox 360 game from its disc files, with the same steps for every game.

usage: python tools/x360port/x360port.py <step> [--port DIR] [--jobs N] [--force]

The generic path of this repository: no game-specific code, only the ReXGlue SDK's own project template (rexglue init)
and its runtime, Xenos GPU emulation included (the ports in games/ replace that with native renderers; a generic port
keeps it). A port folder holds game_files/ (the disc, copied and checked by the launcher). Generated files go to
<port>/project (the ReXGlue project) and <port>/build. Steps (each skips itself when done; --force redoes it):
  sdk        the pinned ReXGlue SDK (tools/x360port/sdk.json), once for the repository (.local/sdk)
  msvcsdk    Windows: the MSVC C/C++ runtime and the Windows SDK the SDK's libraries were built against, fetched
             with xwin (pinned, SHA-256 checked) into .local/xwin, so LLVM (clang + lld) builds without Visual Studio
  learn      functions a run called that the analysis did not find (the runtime's "[FATAL] Call to invalid or
             unregistered function" in build/logs) are declared in the manifest: the next build includes them
  init       rexglue init: the project and manifest for the game's default.xex
  codegen    rexglue codegen: the game's PowerPC code as C++
  prepare    sdk msvcsdk init learn codegen
  compile    compile and link build/game.exe
  play       run the game
"""
import argparse
import hashlib
import json
import re
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
LOCAL = REPO / '.local'
PIN = json.loads((HERE / 'sdk.json').read_text(encoding='utf-8'))
SDK = PIN['sdk']
if sys.platform == 'darwin':
    SDK = {**SDK, **PIN['sdk_mac']}
elif os.name != 'nt':
    SDK = {**SDK, **PIN['sdk_linux']}
EXE = '.exe' if os.name == 'nt' else ''
XWIN_VERSION = '0.10.0'
XWIN_URL = (f'https://github.com/Jake-Shadle/xwin/releases/download/{XWIN_VERSION}/'
            f'xwin-{XWIN_VERSION}-x86_64-pc-windows-msvc.tar.gz')
XWIN_SHA256 = '96e83665ef5c1406aabd0be603a8d08e67e604f36fda630b6807ee948a614e26'
XWIN = LOCAL / 'xwin'
# What a build needs from xwin's output; its last step (a convenience symlink) fails without the Windows
# symlink privilege, which these files do not depend on.
XWIN_NEEDED = ('crt/include/vcruntime.h', 'crt/lib/x86_64/msvcrt.lib', 'sdk/include/um/windows.h',
               'sdk/include/ucrt/stdio.h', 'sdk/lib/um/x86_64/kernel32.lib', 'sdk/lib/ucrt/x86_64/ucrt.lib')
NAME = 'game'   # the CMake target and executable of every generic port


class Port:
    def __init__(self, folder):
        self.dir = Path(folder).resolve()
        self.files = self.dir / 'game_files'
        self.project = self.dir / 'project'
        self.build = self.dir / 'build'
        self.manifest = self.project / f'{NAME}_manifest.toml'
        self.stamp = self.dir / 'build' / 'codegen-complete.json'
        self.exe = self.build / (NAME + EXE)


def log(msg):
    print(msg, flush=True)


def run(cmd, cwd=None):
    log('$ ' + ' '.join(str(c) for c in cmd))
    r = subprocess.run([str(c) for c in cmd], cwd=cwd)
    if r.returncode != 0:
        raise SystemExit(f'failed ({r.returncode}): {Path(str(cmd[0])).name}')


def need(name):
    path = shutil.which(name)
    if not path:
        raise SystemExit(f'{name} was not found: run this through the launcher or Setup (they put the build tools in PATH)')
    return path


def sdk_prefix():
    return LOCAL / 'sdk' / SDK['folder']


def step_sdk(port, args):
    prefix = sdk_prefix()
    if not (prefix / 'lib/cmake/rexglue').is_dir() or args.force:
        archive = LOCAL / 'downloads' / Path(SDK['url']).name
        if not archive.is_file():
            log('Downloading the ReXGlue SDK ' + SDK['version'])
            archive.parent.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(urllib.request.Request(SDK['url'], headers={'User-Agent': 'x360port'}), timeout=600) as r:
                archive.write_bytes(r.read())
        if hashlib.sha256(archive.read_bytes()).hexdigest() != SDK['sha256']:
            archive.unlink()
            raise SystemExit('ReXGlue SDK download damaged or changed (SHA-256 mismatch); run Setup again')
        with zipfile.ZipFile(archive) as z:
            z.extractall(LOCAL / 'sdk')
    if not (prefix / 'lib/cmake/rexglue').is_dir():
        raise SystemExit(f'{prefix} is not a ReXGlue SDK install (lib/cmake/rexglue missing)')
    # The same header patches as the ports (keep host floating-point exceptions masked when the guest writes FPSCR).
    for patch in SDK.get('patches', []):
        header = prefix / patch['file']
        text = header.read_text(encoding='utf-8')
        if patch['new'] not in text:
            if patch['old'] not in text:
                raise SystemExit(f'Unexpected SDK version: cannot apply {patch["name"]} to {header}')
            header.write_text(text.replace(patch['old'], patch['new'], 1), encoding='utf-8')
            log('Applied SDK patch ' + patch['name'])
    log('- ReXGlue SDK: ' + str(prefix))


def msvc_env_present():
    """A Visual Studio developer environment (vcvars) is already loaded."""
    return any((Path(d) / 'vcruntime.h').is_file() for d in os.environ.get('INCLUDE', '').split(';') if d)


def step_msvcsdk(port, args):
    if os.name != 'nt' or msvc_env_present():
        return
    if all((XWIN / f).is_file() for f in XWIN_NEEDED) and not args.force:
        log('- MSVC runtime and Windows SDK (xwin): already fetched')
        return
    import tarfile
    work = LOCAL / 'xwin-dl'
    exe = next(work.glob('*/xwin.exe'), None) if work.is_dir() else None
    if not exe:
        log(f'Downloading xwin {XWIN_VERSION} (fetches the MSVC runtime and Windows SDK from Microsoft)')
        work.mkdir(parents=True, exist_ok=True)
        archive = work / 'xwin.tar.gz'
        with urllib.request.urlopen(urllib.request.Request(XWIN_URL, headers={'User-Agent': 'x360port'}), timeout=300) as r:
            archive.write_bytes(r.read())
        if hashlib.sha256(archive.read_bytes()).hexdigest() != XWIN_SHA256:
            archive.unlink()
            raise SystemExit('xwin download damaged or changed (SHA-256 mismatch); run Setup again')
        with tarfile.open(archive) as t:
            t.extractall(work)
        exe = next(work.glob('*/xwin.exe'))
    # Accepting the license here is what installing the Visual Studio Build Tools would ask for: the same files.
    subprocess.run([str(exe), '--accept-license', '--arch', 'x86_64', '--cache-dir', str(LOCAL / 'xwin-cache'),
                    'splat', '--disable-symlinks', '--output', str(XWIN)])
    missing = [f for f in XWIN_NEEDED if not (XWIN / f).is_file()]
    if missing:
        raise SystemExit('xwin did not produce ' + ', '.join(missing))
    log('- MSVC runtime and Windows SDK: ' + str(XWIN))


def xwin_env():
    """INCLUDE / LIB for clang and lld over the xwin output (what vcvars would set)."""
    inc = ['crt/include', 'sdk/include/ucrt', 'sdk/include/um', 'sdk/include/shared', 'sdk/include/winrt',
           'sdk/include/cppwinrt']
    lib = ['crt/lib/x86_64', 'sdk/lib/um/x86_64', 'sdk/lib/ucrt/x86_64']
    return {'INCLUDE': ';'.join(str(XWIN / d) for d in inc), 'LIB': ';'.join(str(XWIN / d) for d in lib)}


def rexglue():
    return sdk_prefix() / 'bin' / ('rexglue' + EXE)


def step_init(port, args):
    if port.manifest.is_file() and not args.force:
        log('- ReXGlue project: already created')
        return
    xex = port.files / 'default.xex'
    if not xex.is_file():
        raise SystemExit(f'{xex} is missing: install the game files first (the launcher does it from your disc)')
    if port.project.exists():
        shutil.rmtree(port.project)
    run([rexglue(), '-f', 'init', '--project-name', NAME, '--xex-path', xex, '--game-root', port.files,
         '--project-root', port.project, '--scan-dll'], cwd=port.dir)


RUNTIME_MISSING = re.compile(r'Call to invalid or unregistered function at guest address (0x[0-9A-Fa-f]{8})')
UNRESOLVED = re.compile(r'^\s*(0x[0-9A-Fa-f]{8}) from 0x[0-9A-Fa-f]+: .*target not in any function', re.M)


def declare_functions(manifest, addresses):
    """Adds entry points to [entrypoint.functions] of the manifest (rexglue finds their end itself)."""
    text = manifest.read_text(encoding='utf-8')
    have = set(a.lower() for a in re.findall(r'^(0x[0-9A-Fa-f]+)\s*=', text, re.M))
    new = []
    for a in addresses:
        if a.lower() not in have:
            have.add(a.lower())
            new.append('0x' + a[2:].upper())
    if not new:
        return 0
    lines = ''.join(f'{a} = {{}}\n' for a in new)
    if '[entrypoint.functions]' in text:
        text = text.replace('[entrypoint.functions]\n', '[entrypoint.functions]\n' + lines, 1)
    else:
        text = text.rstrip('\n') + '\n\n[entrypoint.functions]\n# Entry points the analysis missed: branch targets outside every function and calls a run reported (x360port)\n' + lines
    manifest.write_text(text, encoding='utf-8')
    return len(new)


def step_learn(port, args):
    logs = sorted((port.build / 'logs').glob('*.log')) if (port.build / 'logs').is_dir() else []
    found = []
    for f in logs:
        found += RUNTIME_MISSING.findall(f.read_text(encoding='utf-8', errors='replace'))
    if not found or not port.manifest.is_file():
        log('- functions found missing by a run: nothing new')
        return
    added = declare_functions(port.manifest, found)
    if added:
        port.stamp.unlink(missing_ok=True)       # the translation runs again with them
        log(f'{added} function(s) a run found missing declared in {port.manifest.name}')
    else:
        log('- functions found missing by a run: all declared already')


def step_codegen(port, args):
    if port.stamp.is_file() and not args.force:
        log('- translated code: already done')
        return
    port.stamp.unlink(missing_ok=True)
    # Branch targets outside every function stop the analysis ("UnresolvedCall"): they are declared as functions
    # and the translation runs again, until it passes or stops finding new ones.
    for attempt in range(8):
        cmd = [str(rexglue()), 'codegen', str(port.manifest)]
        log('$ ' + ' '.join(cmd))
        proc = subprocess.Popen(cmd, cwd=port.project, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, encoding='utf-8', errors='replace')
        output = []
        for line in proc.stdout:
            print(line, end='', flush=True)
            output.append(line)
        if proc.wait() == 0:
            break
        added = declare_functions(port.manifest, UNRESOLVED.findall(''.join(output)))
        if not added:
            raise SystemExit(f'failed ({proc.returncode}): rexglue codegen')
        log(f'{added} branch target(s) declared as functions in {port.manifest.name}; translating again')
    else:
        raise SystemExit('rexglue codegen kept finding new targets; see the log')
    port.build.mkdir(parents=True, exist_ok=True)
    xex = port.files / 'default.xex'
    port.stamp.write_text(json.dumps({'default.xex': hashlib.sha256(xex.read_bytes()).hexdigest()}) + '\n', encoding='utf-8')


def step_prepare(port, args):
    for step in (step_sdk, step_msvcsdk, step_init, step_learn, step_codegen):
        step(port, args)


def step_compile(port, args):
    clang, clangxx = need('clang' + EXE), need('clang++' + EXE)
    if os.name != 'nt':
        clang = shutil.which('clang-20') or clang
        clangxx = shutil.which('clang++-20') or clangxx
    need('cmake'), need('ninja')
    extra = []
    if os.name == 'nt' and not msvc_env_present():
        # LLVM alone: clang for the MSVC target with lld, over the MSVC runtime and Windows SDK from xwin.
        step_msvcsdk(port, args)
        os.environ.update(xwin_env())
        extra = ['-DCMAKE_LINKER_TYPE=LLD', '-DCMAKE_RC_COMPILER=llvm-rc',
                 '-DCMAKE_TRY_COMPILE_CONFIGURATION=Release']   # xwin brings the release runtime only
    run(['cmake', '-S', port.project, '-B', port.build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
         '-DCMAKE_C_COMPILER=' + clang, '-DCMAKE_CXX_COMPILER=' + clangxx, *extra,
         '-DCMAKE_PREFIX_PATH=' + sdk_prefix().as_posix(), '-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=' + port.build.as_posix()])
    run(['cmake', '--build', port.build, '--parallel', str(args.jobs), '--target', NAME])


def step_play(port, args):
    if not port.exe.is_file():
        raise SystemExit('The game is not built yet: install it from the launcher.')
    r = subprocess.run([str(port.exe), '--game_data_root=' + str(port.files)] + args.extra, cwd=port.exe.parent)
    sys.exit(r.returncode)


STEPS = {'sdk': step_sdk, 'msvcsdk': step_msvcsdk, 'init': step_init, 'learn': step_learn, 'codegen': step_codegen, 'prepare': step_prepare,
         'compile': step_compile, 'play': step_play,
         'all': lambda p, a: (step_prepare(p, a), step_compile(p, a))}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('step', choices=STEPS)
    ap.add_argument('--port', default='.')
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument('--force', action='store_true')
    # play: any other option goes to the game (e.g. --fullscreen=true).
    args, args.extra = ap.parse_known_args()
    if args.extra and args.step != 'play':
        ap.error('unknown options: ' + ' '.join(args.extra))
    STEPS[args.step](Port(args.port), args)


if __name__ == '__main__':
    main()
