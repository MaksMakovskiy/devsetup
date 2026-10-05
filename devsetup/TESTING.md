# Verification report

## Verified environments

- Windows x64, MSVC Build Tools 2022 (14.44.35207).
- Ubuntu 26.04.1 LTS x86_64 under WSL2, GCC 15.2.
- Linux debug build with AddressSanitizer and UndefinedBehaviorSanitizer.

## Results

| Feature | Result | Method |
| --- | --- | --- |
| OS, architecture, compiler and VS Code discovery | Passed | Real MSVC/GCC installations; isolated environments with tools absent |
| Windows winget alias detection | Passed | Real Windows App Execution Alias; discovery without WindowsApps in PATH |
| Main menu, invalid input, EOF, UTF-8 BOM | Passed | Scripted console input |
| CLI argument validation | Passed | Valid options and missing/extra/unknown arguments |
| MSVC installation command | Passed, mocked | Captured package ID and complete workload override argument |
| GCC/Clang installation commands | Passed, mocked | apt, dnf, pacman and zypper; both compiler choices |
| VS Code installation command | Passed, mocked | winget and Snap |
| C/C++ extension installation command | Passed, mocked | Captured exact extension ID; paths with spaces and special characters |
| Installation confirmation and cancellation | Passed | Verified no mocked installer invocation after cancellation |
| Installer failures | Passed | Nonzero exit propagated and no success message; apt update failure prevents install |
| Project configuration and valid JSON | Passed | Real generated VS Code task and IntelliSense settings |
| Compilation and execution | Passed | Real MSVC/GCC hello-world builds through generated task commands |
| Compiler error propagation | Passed | Intentionally invalid source returns a failed build |
| Paths with spaces, Unicode and `&` | Passed | Actual builds on Windows and Linux; apostrophe also tested on Linux |
| Repeated configuration | Passed | Identical files remain unchanged |
| Conflict cancellation | Passed | Declining a later conflict leaves all original settings unchanged |
| Backup creation and numbering | Passed | Original contents preserved in `.bak` and `.bak.1` |
| Existing extension recommendations | Passed | Custom `extensions.json` preserved |
| Invalid project destination | Passed | Clean failure; existing file preserved |
| Release archives and checksums | Passed | Extracted Windows ZIP, Linux tar.gz and DEB payload; SHA-256 comparison |

The audit fixed acceptance of extra arguments after `--help`. Regression tests now
require every documented CLI option to have the correct argument count.

## Reproduce

Run these commands from the `devsetup` source directory.

Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File test-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File test-functional-windows.ps1
```

Linux:

```sh
bash build-linux.sh
bash test-packages.sh
# Optional sanitizer run:
mkdir -p build
g++ -std=c++17 -g -fsanitize=address,undefined src/main.cpp -o build/devsetup-audit
python3 test-linux.py build/devsetup-audit
python3 test-functional-linux.py build/devsetup-audit
```

`build-linux.sh` runs both Linux test suites before packaging. The package check
requires the release files and matching `dist/SHA256SUMS.txt`.
Windows test artifacts are written under ignored `test-output`; Linux test directories
are temporary. Mock installers do not install or remove software.

## Limits

- Actual compiler/editor/extension downloads, administrator prompts, installation,
  upgrades and reboots were not performed. Installer command construction and handling
  were tested with replacements that log calls and return controlled status codes.
- Tests execute the exact generated VS Code task command; they do not automate the
  VS Code graphical interface or validate IntelliSense suggestions visually.
- macOS, Windows ARM, 32-bit systems and other Linux distributions were not run.
  dnf/pacman/zypper command tests do not imply testing on those distributions.
- The DEB was inspected and its payload extracted and executed; it was not installed
  into the host package database.
- Building one active C++17 file is the supported workflow. Multi-file projects and
  debugger configuration remain outside the current feature set.
