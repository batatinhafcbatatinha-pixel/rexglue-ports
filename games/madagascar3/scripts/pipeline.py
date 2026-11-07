"""ReXGlue pipeline with a bounded project output directory."""
import argparse
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'project'
SDK = ROOT.parent / 'battlefield2/third_party/rexglue-sdk/win-amd64'
GIB = 1024 ** 3
BUILD_ROOT = Path(os.environ.get('MADAGASCAR_BUILD_ROOT',
    str(ROOT / 'project/out/build'))).resolve()


def project_bytes():
    size = sum(p.stat().st_size for p in PROJECT.rglob('*') if p.is_file())
    if not BUILD_ROOT.is_relative_to(PROJECT):
        size += sum(p.stat().st_size for p in BUILD_ROOT.rglob('*') if p.is_file())
    return size


def check_disk():
    if project_bytes() >= 2 * GIB:
        raise RuntimeError('Project output reached the 2 GiB limit')


def run(command, log):
    check_disk()
    print('Running:', subprocess.list2cmdline(list(map(str, command))), flush=True)
    with (ROOT / 'analysis' / log).open('w', encoding='utf-8') as output:
        proc = subprocess.Popen(list(map(str, command)), cwd=PROJECT,
                                stdout=output, stderr=subprocess.STDOUT)
        try:
            while proc.poll() is None:
                check_disk()
                time.sleep(2)
        except BaseException:
            subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                           capture_output=True)
            proc.wait()
            raise
        if proc.returncode:
            raise RuntimeError(f'Command failed ({proc.returncode}); see analysis/{log}')
    check_disk()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('configuration', nargs='?', choices=['release', 'debug'], default='release')
    ap.add_argument('stage', nargs='?', choices=['codegen', 'configure', 'build'], default='build',
                    help='Stop after this stage; default runs all stages')
    args = ap.parse_args()
    check_disk()
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    temporary = BUILD_ROOT / 'compiler-temp'
    temporary.mkdir(exist_ok=True)
    os.environ['TMP'] = os.environ['TEMP'] = str(temporary)
    run([SDK / 'bin/rexglue.exe', 'codegen', 'madagascar3_manifest.toml'], 'codegen.log')
    if args.stage == 'codegen':
        return
    preset = 'win-amd64-' + args.configuration
    binary_dir = BUILD_ROOT / preset
    run(['cmake', '--preset', preset, '-B', binary_dir, f'-DCMAKE_PREFIX_PATH={SDK}',
         '-DREXGLUE_RECOMP_DEBUG_INFO=none'], 'configure.log')
    if args.stage == 'configure':
        return
    run(['cmake', '--build', binary_dir, '--parallel', '2'], 'build.log')
    (ROOT / 'build-location.txt').write_text(str(binary_dir) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
