#!/usr/bin/env python3
"""Exercise the real generated build task without installing any packages."""
import json
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()

def run(*args, **kwargs):
    return subprocess.run([str(binary), *args], check=True, **kwargs)

run('--check')
run('--help')
with tempfile.TemporaryDirectory(prefix='devsetup test ') as directory:
    root = pathlib.Path(directory)
    project = root / "project with spaces & apostrophe ' Unicode \u0442\u0435\u0441\u0442"
    run('--configure', str(project))
    source = project / 'hello.cpp'
    source.write_text('#include <iostream>\nint main() { std::cout << "smoke-ok"; }\n')
    tasks_path = project / '.vscode/tasks.json'
    task = json.loads(tasks_path.read_text())['tasks'][0]
    properties = json.loads((project / '.vscode/c_cpp_properties.json').read_text())
    assert pathlib.Path(properties['configurations'][0]['compilerPath']).is_file()
    variables = {
        '${workspaceFolder}': str(project),
        '${fileDirname}': str(project),
        '${fileBasenameNoExtension}': 'hello',
        '${file}': str(source),
    }
    def expand(value):
        for key, replacement in variables.items():
            value = value.replace(key, replacement)
        return value
    subprocess.run([task['command'], *map(expand, task['args'])], cwd=project, check=True)
    assert subprocess.check_output([str(project / 'hello')], text=True) == 'smoke-ok'
    source.write_text('#error expected compile failure\n')
    failure = subprocess.run([task['command'], *map(expand, task['args'])], cwd=project,
                             capture_output=True)
    assert failure.returncode != 0
    before = tasks_path.read_bytes()
    run('--configure', str(project))
    assert tasks_path.read_bytes() == before
    tasks_path.write_text('{"custom":true}')
    conflict = subprocess.run([str(binary), '--configure', str(project)])
    assert conflict.returncode != 0
    assert tasks_path.read_text() == '{"custom":true}'
    result = run(input='invalid\n0\n', text=True, capture_output=True)
    assert 'Enter a menu option number.' in result.stdout
    invalid = subprocess.run([str(binary), '--invalid'], capture_output=True)
    assert invalid.returncode != 0
print('PASS: Linux detection, JSON, generated task, compile/run, idempotence, conflict protection, input.')
