#!/usr/bin/env python3
"""Isolated menu/installer regression tests. No real installations or sudo."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
count = 0
with tempfile.TemporaryDirectory(prefix="devsetup functional '") as tmp:
    root = Path(tmp)
    log = root / 'calls.txt'

    def environment(names=(), status=0):
        global count
        count += 1
        tools = root / f'tools {count}'
        tools.mkdir()
        for name in names:
            script = tools / name
            script.write_text(
                '#!/bin/sh\n'
                + ('exec "$@"\n' if name == 'sudo' else
                   f'printf "%s\\n" "{name}:$*" >> "$DEVSETUP_LOG"\nexit {status}\n')
            )
            script.chmod(0o755)
        log.write_text('')
        return dict(os.environ, PATH=str(tools), DEVSETUP_LOG=str(log))

    def run(inputs='', args=(), env=None, expected=0):
        result = subprocess.run([str(binary), *args], input=inputs, text=True,
                                capture_output=True, env=env, timeout=15)
        assert result.returncode == expected, (result.returncode, result.stdout, result.stderr)
        return result.stdout + result.stderr

    empty = environment()
    assert 'No compiler found' in run(args=['--check'], env=empty)
    assert 'Install VS Code first' in run('5\n0\n', env=empty)
    assert 'Install a compiler first' in run('4\n0\n', env=empty)
    assert 'No compiler found' in run(args=['--configure', str(root / 'missing')], env=empty, expected=1)
    assert not (root / 'missing').exists()
    run(args=['--configure'], env=empty, expected=1)
    run(args=['--check', 'extra'], env=empty, expected=1)
    run(args=['--help', 'extra'], env=empty, expected=1)
    assert 'End of input' in run(env=empty, expected=1)
    assert 'Enter a menu option number' in run('invalid\n0\n', env=empty)
    run('\ufeff0\n', env=empty)

    for manager, gcc, clang in [
        ('apt-get', 'install build-essential gdb', 'install clang build-essential gdb'),
        ('dnf', 'install gcc-c++ gdb', 'install clang gcc-c++ gdb'),
        ('pacman', '-Syu --needed gcc gdb', '-Syu --needed clang gcc gdb'),
        ('zypper', 'install gcc-c++ gdb', 'install clang gcc-c++ gdb'),
    ]:
        for choice, expected in [('1', gcc), ('2', clang)]:
            env = environment([manager, 'sudo'])
            output = run(f'2\n{choice}\ny\n0\n', env=env)
            assert f'{manager}:{expected}' in log.read_text(), output
            assert 'Exit code: 0' in output
        env = environment([manager, 'sudo'])
        run('2\n1\nn\n0\n', env=env)
        assert log.read_text() == ''
        run('2\n0\n0\n', env=env)
        assert log.read_text() == ''

    env = environment(['apt-get', 'sudo'], status=23)
    output = run('2\n1\ny\n0\n', env=env)
    assert 'Exit code: 23' in output and 'Request completed' not in output
    assert log.read_text().splitlines() == ['apt-get:update']
    env = environment(['snap', 'sudo'])
    run('3\n1\ny\n0\n', env=env)
    assert log.read_text().strip() == 'snap:install code --classic'
    env = environment(['sudo'])
    assert 'Download VS Code:' in run('3\n0\n', env=env)
    env = environment(['code'])
    run('5\ny\n0\n', env=env)
    assert log.read_text().strip() == 'code:--install-extension ms-vscode.cpptools'
    log.write_text('')
    run('5\nn\n0\n', env=env)
    assert log.read_text() == ''
    env = environment(['code'], status=23)
    assert 'Exit code: 23' in run('5\ny\n0\n', env=env)

    # Real compiler detection, but no compilation required for backup behavior.
    env = environment(['code'])
    (Path(env['PATH']) / 'g++').symlink_to('/usr/bin/g++')
    project = root / 'project Unicode \u0442\u0435\u0441\u0442 & spaces'
    run(args=['--configure', str(project)], env=env)
    settings = project / '.vscode'
    tasks = settings / 'tasks.json'
    props = settings / 'c_cpp_properties.json'
    ext = settings / 'extensions.json'
    ext.write_text('{"recommendations":["custom.extension"]}')
    tasks.write_text('first custom tasks')
    props.write_text('custom properties')
    run(f'4\n1\n{project}\ny\nn\n0\n', env=env)
    assert tasks.read_text() == 'first custom tasks'
    assert props.read_text() == 'custom properties'
    assert not list(settings.glob('*.bak*'))
    run(f'4\n1\n{project}\ny\ny\n0\n', env=env)
    assert (settings / 'tasks.json.bak').read_text() == 'first custom tasks'
    assert (settings / 'c_cpp_properties.json.bak').read_text() == 'custom properties'
    json.loads(tasks.read_text())
    assert json.loads(ext.read_text())['recommendations'] == ['custom.extension']
    tasks.write_text('second custom tasks')
    run(f'4\n1\n{project}\ny\n0\n', env=env)
    assert (settings / 'tasks.json.bak.1').read_text() == 'second custom tasks'
    assert (settings / 'tasks.json.bak').read_text() == 'first custom tasks'
    bad = root / 'not-a-directory'
    bad.write_text('preserve me')
    run(args=['--configure', str(bad)], env=env, expected=1)
    assert bad.read_text() == 'preserve me'

print('PASS: Linux menu, all supported package managers (mocked), confirmations, failures, extension, backups, missing tools, CLI, Unicode paths.')
