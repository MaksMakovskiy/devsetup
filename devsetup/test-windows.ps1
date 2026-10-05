param([string]$Executable = (Join-Path $PSScriptRoot 'dist/devsetup-windows-x64.exe'))
$ErrorActionPreference = 'Stop'
$exe = [IO.Path]::GetFullPath($Executable)
$project = Join-Path $PSScriptRoot ('test-output/project & spaces ' + [char]0x0442 + [char]0x0435 + [char]0x0441 + [char]0x0442 + ' ' + [guid]::NewGuid().ToString('N'))
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
    [IO.File]::WriteAllText($source, '#error expected compile failure')
    & $task.command @taskArgs 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { throw 'Compiler error was reported as success' }
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

# Exercise installation command quoting without running a real installer.
$mockBin = Join-Path $project 'mock tools'
New-Item -ItemType Directory -Path $mockBin -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $mockBin 'winget.cmd'), "@echo off`r`necho MOCK-WINGET %*`r`nexit /b 0`r`n", [Text.Encoding]::ASCII)
$startInfo = New-Object Diagnostics.ProcessStartInfo
$startInfo.FileName = $exe
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.EnvironmentVariables['PATH'] = $mockBin + ';' + $env:PATH
$child = [Diagnostics.Process]::Start($startInfo)
$inputBytes = [Text.Encoding]::ASCII.GetBytes("2`n1`ny`n0`n")
$child.StandardInput.BaseStream.Write($inputBytes, 0, $inputBytes.Length)
$child.StandardInput.BaseStream.Flush()
$child.StandardInput.Close()
$menuText = $child.StandardOutput.ReadToEnd()
$errors = $child.StandardError.ReadToEnd()
$child.WaitForExit()
if ($child.ExitCode -ne 0 -or $errors) { throw "Mock installer failed: $errors" }
if (!$menuText.Contains('Use option 4 to configure your project')) { throw "Missing existing compiler guidance: $menuText" }
if (!$menuText.Contains('MOCK-WINGET install --exact --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"')) { throw "Incorrect winget invocation: $menuText" }
Write-Host 'PASS: installed-compiler guidance and quoted winget invocation (mock only).'
