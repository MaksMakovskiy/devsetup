$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'dist/devsetup-windows-x64.exe'
$project = Join-Path $PSScriptRoot ('test-output/project with spaces ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $project -Force | Out-Null
& $exe --check
if ($LASTEXITCODE -ne 0) { throw 'Detection failed' }
& $exe --configure $project
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
$tasks = Get-Content -LiteralPath (Join-Path $project '.vscode/tasks.json') -Raw | ConvertFrom-Json
$properties = Get-Content -LiteralPath (Join-Path $project '.vscode/c_cpp_properties.json') -Raw | ConvertFrom-Json
if (!(Test-Path -LiteralPath $properties.configurations[0].compilerPath)) { throw 'Invalid compilerPath' }
$source = Join-Path $project 'hello.cpp'
$output = Join-Path $project 'hello.exe'
[IO.File]::WriteAllText($source, '#include <iostream>' + "`n" + 'int main() { std::cout << "smoke-ok"; }')
$task = $tasks.tasks[0]
$taskArgs = @($task.args | ForEach-Object {
    $_.Replace('${workspaceFolder}', $project).Replace('${fileDirname}', $project).Replace('${fileBasenameNoExtension}', 'hello').Replace('${file}', $source)
})
Push-Location $project
try {
    & $task.command @taskArgs
    if ($LASTEXITCODE -ne 0) { throw 'Generated VS Code task failed' }
    $result = & $output
    if ($LASTEXITCODE -ne 0 -or $result -ne 'smoke-ok') { throw 'Compiled program failed' }
} finally { Pop-Location }
# Idempotence and refusal to overwrite existing custom settings.
& $exe --configure $project
if ($LASTEXITCODE -ne 0) { throw 'Idempotent configuration failed' }
$settings = Join-Path $project '.vscode/tasks.json'
[IO.File]::WriteAllText($settings, '{"custom":true}')
& $exe --configure $project
if ($LASTEXITCODE -eq 0) { throw 'Conflict was not rejected' }
if ([IO.File]::ReadAllText($settings) -ne '{"custom":true}') { throw 'Existing settings changed' }
$menu = 'invalid', '0' | & $exe
if ($LASTEXITCODE -ne 0) { throw 'Menu handling failed' }
& $exe --invalid
if ($LASTEXITCODE -eq 0) { throw 'Invalid arguments accepted' }
Write-Host 'PASS: detection, JSON, generated task, compile/run, idempotence, conflict protection, menu, invalid arguments.'
