#!/usr/bin/env python3
"""Native profile entry point. All build products remain private."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]


def environment(check_tools=True):
    if sys.platform not in ('linux', 'darwin'):
        raise ValueError('Use the Windows entry point on this system.')
    if platform.machine().lower() not in ('x86_64', 'amd64'):
        raise ValueError('The pinned SDK supports x86-64 only; no native ARM SDK is available.')
    if sys.platform == 'darwin':
        translated = subprocess.run(['sysctl', '-in', 'sysctl.proc_translated'],
                                    capture_output=True, text=True)
        if translated.stdout.strip() == '1':
            raise ValueError('Native builds require an Intel Mac; translated processes are unsupported.')
    env = os.environ.copy()
    tools = Path(env.get('PORT_TOOLS_DIR', REPO / '.local/tools'))
    paths = [tools / 'cmake-3.31.12/bin', tools / 'cmake-3.31.12/CMake.app/Contents/bin',
             tools / 'ninja-1.13.2']
    if sys.platform == 'darwin':
        paths += [Path('/usr/local/opt/llvm/bin')]
    env['PATH'] = os.pathsep.join([str(p) for p in paths if p.is_dir()] + [env.get('PATH', '')])
    for name in (('cmake', 'ninja', 'pkg-config') if check_tools else ()):
        if not shutil.which(name, path=env['PATH']):
            raise ValueError(name + ' is missing; install the launcher dependencies.')
    return env


def dependencies(env):
    base = REPO / '.local/tools/native' / ('macos' if sys.platform == 'darwin' else 'linux')
    vkd3d = Path(env.get('VKD3D_DIR', base / 'vkd3d')).resolve()
    key = 'DXMT_DIR' if sys.platform == 'darwin' else 'DXVK_NATIVE_DIR'
    graphics = Path(env.get(key, base / ('dxmt' if sys.platform == 'darwin' else 'dxvk'))).resolve()
    required = graphics / ('lib/d3d11.dylib' if sys.platform == 'darwin' else 'include/dxvk/d3d11.h')
    if not required.is_file():
        raise ValueError(f'{key} must point at a native graphics prefix containing {required.relative_to(graphics)}. See docs/LINUX_MACOS.md.')
    pkg = [vkd3d / d / 'pkgconfig' for d in ('lib', 'lib64', 'lib/x86_64-linux-gnu')]
    env['PKG_CONFIG_PATH'] = os.pathsep.join(map(str, pkg)) + os.pathsep + env.get('PKG_CONFIG_PATH', '')
    subprocess.run(['pkg-config', '--atleast-version=1.19', 'libvkd3d-shader'], env=env, check=True)
    subprocess.run(['pkg-config', '--exists', 'fontconfig', 'freetype2'], env=env, check=True)
    return ['--vkd3d-dir', str(vkd3d), '--dxmt-dir' if sys.platform == 'darwin' else '--dxvk-native-dir', str(graphics)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('build', 'play', 'check'))
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--game-files', type=Path)
    parser.add_argument('--jobs', type=int, default=int(os.environ.get('JOBS', '2')))
    args, extra = parser.parse_known_args()
    root = args.profile.resolve()
    if not (root / 'kit.json').is_file():
        raise ValueError('This profile has no verified portable build kit; see docs/LINUX_MACOS.md.')
    env = environment(check_tools=args.action != 'play')
    if args.action == 'check':
        dependencies(env)
        return
    files = (args.game_files or root / 'game_files').resolve()
    if args.action == 'build':
        command = [sys.executable, str(root / 'tools/build_local.py'), '--game-files', str(files),
                   '--jobs', str(args.jobs), *dependencies(env), *extra]
    else:
        if extra[:1] == ['--']:
            extra = extra[1:]
        command = [sys.executable, str(root / 'tools/play.py'), '--game-files', str(files), '--', *extra]
    raise SystemExit(subprocess.call(command, cwd=root, env=env))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
