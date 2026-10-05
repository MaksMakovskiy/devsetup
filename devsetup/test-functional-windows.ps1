param([string]$Executable = (Join-Path $PSScriptRoot 'dist/devsetup-windows-x64.exe'))
$ErrorActionPreference = 'Stop'
$exe = [IO.Path]::GetFullPath($Executable)
$root = Join-Path $PSScriptRoot ('test-output/functional & spaces ' + [guid]::NewGuid().ToString('N'))
$mockBin = Join-Path $root 'mock tools'
New-Item -ItemType Directory -Force -Path $mockBin | Out-Null
$log = Join-Path $root 'calls.txt'
$mockEnv = @{ PATH=$mockBin; DEVSETUP_LOG=$log }
$noTools = @{
    PATH=(Join-Path $root 'missing'); LOCALAPPDATA=(Join-Path $root 'missing')
    'ProgramFiles(x86)'=(Join-Path $root 'missing'); ProgramFiles=(Join-Path $root 'missing')
}
function Invoke-Test([string]$InputText, [string]$Arguments='', [hashtable]$Environment=$mockEnv, [int]$Expected=0) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $exe
    $info.Arguments = $Arguments
    $info.UseShellExecute = $false
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::Start($info)
    $outputTask = $child.StandardOutput.ReadToEndAsync()
    $errorTask = $child.StandardError.ReadToEndAsync()
    $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($InputText)
    $child.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
    $child.StandardInput.BaseStream.Flush()
    $child.StandardInput.Close()
    if (!$child.WaitForExit(20000)) { $child.Kill(); throw 'Test process timeout' }
    $output = $outputTask.Result + $errorTask.Result
    if ($child.ExitCode -ne $Expected) { throw "Unexpected exit $($child.ExitCode): $output" }
    return $output
}
function Assert-Contains([string]$Text, [string]$Expected) {
    if (!$Text.Contains($Expected)) { throw "Missing '$Expected': $Text" }
}
function Write-Mock([string]$Name, [int]$Status=0) {
    $content = '@echo off' + "`r`n" + 'echo ' + $Name + ':%*>>"%DEVSETUP_LOG%"' + "`r`nexit /b $Status`r`n"
    [IO.File]::WriteAllText((Join-Path $mockBin ($Name + '.cmd')), $content, [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($log, '')
}

Assert-Contains (Invoke-Test '' '--check' $noTools) 'No compiler found'
Assert-Contains (Invoke-Test "2`n0`n" '' $noTools) 'winget was not found'
Assert-Contains (Invoke-Test "3`n0`n" '' $noTools) 'Download VS Code:'
Assert-Contains (Invoke-Test "5`n0`n" '' $noTools) 'Install VS Code first'
Assert-Contains (Invoke-Test "4`n0`n" '' $noTools) 'Install a compiler first'
Invoke-Test '' ('--configure "' + (Join-Path $root 'missing-project') + '"') $noTools 1 | Out-Null
foreach ($arg in @('--invalid','--configure','--check extra','--help extra')) { Invoke-Test '' $arg $noTools 1 | Out-Null }
Assert-Contains (Invoke-Test '' '' $noTools 1) 'End of input'
Assert-Contains (Invoke-Test "invalid`n0`n" '' $noTools) 'Enter a menu option number'
Invoke-Test (([char]0xFEFF) + "0`n") '' $noTools | Out-Null

Write-Mock 'winget'
Assert-Contains (Invoke-Test "2`n1`ny`n0`n") 'Exit code: 0'
Assert-Contains ([IO.File]::ReadAllText($log)) 'Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"'
Write-Mock 'winget'
Invoke-Test "3`n1`ny`n0`n" | Out-Null
Assert-Contains ([IO.File]::ReadAllText($log)) 'winget:install --exact --id Microsoft.VisualStudioCode'
Write-Mock 'winget'
Invoke-Test "2`n1`nn`n3`n0`n0`n" | Out-Null
if ([IO.File]::ReadAllText($log)) { throw 'Cancelled installer was executed' }
Write-Mock 'winget' 23
$failure = Invoke-Test "3`n1`ny`n0`n"
Assert-Contains $failure 'Exit code: 23'
if ($failure.Contains('Request completed')) { throw 'Installer failure reported as success' }
Write-Mock 'code'
Invoke-Test "5`ny`n0`n" | Out-Null
Assert-Contains ([IO.File]::ReadAllText($log)) 'code:--install-extension ms-vscode.cpptools'
Write-Mock 'code'
Invoke-Test "5`nn`n0`n" | Out-Null
if ([IO.File]::ReadAllText($log)) { throw 'Cancelled extension install was executed' }
Write-Mock 'code' 23
Assert-Contains (Invoke-Test "5`ny`n0`n") 'Exit code: 23'
Write-Mock 'code'

$project = Join-Path $root ('project ' + [char]0x0442 + [char]0x0435 + [char]0x0441 + [char]0x0442)
Invoke-Test '' ('--configure "' + $project + '"') | Out-Null
$settings = Join-Path $project '.vscode'
$tasks = Join-Path $settings 'tasks.json'
$props = Join-Path $settings 'c_cpp_properties.json'
$ext = Join-Path $settings 'extensions.json'
[IO.File]::WriteAllText($ext, '{"recommendations":["custom.extension"]}')
[IO.File]::WriteAllText($tasks, 'first custom tasks')
[IO.File]::WriteAllText($props, 'custom properties')
Invoke-Test "4`n1`n$project`ny`nn`n0`n" | Out-Null
if ([IO.File]::ReadAllText($tasks) -ne 'first custom tasks' -or (Get-ChildItem -LiteralPath $settings -Filter '*.bak*')) { throw 'Declining a conflict changed files' }
Invoke-Test "4`n1`n$project`ny`ny`n0`n" | Out-Null
if ([IO.File]::ReadAllText($tasks + '.bak') -ne 'first custom tasks') { throw 'Incorrect backup' }
if ([IO.File]::ReadAllText($props + '.bak') -ne 'custom properties') { throw 'Incorrect properties backup' }
if ([IO.File]::ReadAllText($ext) -ne '{"recommendations":["custom.extension"]}') { throw 'Extension recommendations changed' }
[IO.File]::WriteAllText($tasks, 'second custom tasks')
Invoke-Test "4`n1`n$project`ny`n0`n" | Out-Null
if ([IO.File]::ReadAllText($tasks + '.bak.1') -ne 'second custom tasks') { throw 'Numbered backup missing' }
if ([IO.File]::ReadAllText($tasks + '.bak') -ne 'first custom tasks') { throw 'Previous backup overwritten' }
$bad = Join-Path $root 'not-a-directory'
[IO.File]::WriteAllText($bad, 'preserve me')
Invoke-Test '' ('--configure "' + $bad + '"') $mockEnv 1 | Out-Null
if ([IO.File]::ReadAllText($bad) -ne 'preserve me') { throw 'Invalid target corrupted' }
Write-Host 'PASS: Windows menu, missing tools, compiler/editor/extension install commands (mocked), cancellations, failures, backups, CLI and Unicode paths.'
