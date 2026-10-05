# DevSetup

Source code and build scripts are in [devsetup/](devsetup/). Run build and test commands from that directory: `cd devsetup`.

A C++17 console utility for setting up a C++ compiler and Visual Studio Code.
Ready-to-run binary: `dist/devsetup-windows-x64.exe` (Windows x64, static CRT).
Double-click it or launch it from a terminal. The utility itself does not need a compiler.

## Downloads

These links point to the binaries included in this repository. On GitHub, open a file
and click **Download raw file** if the browser shows a file preview.

| Your system | Download | How to run |
| --- | --- | --- |
| Windows x64 | [Windows ZIP](devsetup/dist/devsetup-windows-x64.zip) or [EXE](devsetup/dist/devsetup-windows-x64.exe) | Extract the ZIP and double-click the EXE |
| Ubuntu / Debian / Linux Mint, amd64 | [DEB package](devsetup/dist/devsetup_0.1.0_amd64.deb) | Install with the command below, then run `devsetup` |
| Other Linux distributions, x86_64 | [Linux tar.gz](devsetup/dist/devsetup-linux-x64.tar.gz) | Extract and run; no package installation needed |

[SHA-256 checksums](devsetup/dist/SHA256SUMS.txt) are provided for the release files.
Ubuntu is a Linux distribution: both Linux downloads contain the same executable.
The `.deb` installs it to `/usr/bin/devsetup`; the archive can be extracted anywhere.

Ubuntu / Debian / Linux Mint:

```sh
sudo apt install ./devsetup_0.1.0_amd64.deb
devsetup
# To uninstall later:
sudo apt remove devsetup
```

Fedora / Arch / openSUSE or portable use on Ubuntu:

```sh
tar -xzf devsetup-linux-x64.tar.gz
chmod +x devsetup-linux-x64
./devsetup-linux-x64
```

The Linux x64 executable is statically linked, including its C/C++ runtime libraries.
It was built and smoke-tested on Ubuntu 26.04.1 LTS in WSL2. Other distributions and
older Ubuntu releases are compatibility targets, not independently verified platforms.
ARM64, 32-bit systems, macOS binaries, and phones are not included in this release.

## Quick start

1. Launch DevSetup. It reports the platform and available tools.
2. If no compiler is found, choose **2 - Install a compiler**.
3. If VS Code is missing, choose **3 - Install VS Code**.
4. Choose **4 - Configure a project for VS Code**, select a compiler, and enter your project folder without quotes.
5. Choose **5 - Install the C/C++ extension**.
6. Open that folder in VS Code, open a C++ source file, and press **Ctrl+Shift+B** (macOS: **Cmd+Shift+B**).
7. Run the resulting executable from the terminal, for example `./hello.exe` on Windows or `./hello` on Linux/macOS.

Menu option **1** repeats detection; **0** exits.
Installation commands are displayed before execution and require `y` or `yes`.
Pressing Enter at a confirmation prompt means no.

If no compiler is found, option 4 offers installation. After installation, you may need
to restart the utility and terminal to refresh PATH. Launching DevSetup does not install packages.
Downloads require internet access; the selected package manager handles administrator permissions.

## How it works

The source uses platform macros to select the Windows, Linux, macOS, or Android code
when it is compiled. Each binary is built for one operating system and architecture;
it does not switch between operating systems at runtime.

At startup, it searches PATH for compilers and package managers. On Windows, it also
uses Visual Studio's `vswhere.exe` to locate an MSVC installation and checks common
VS Code installation paths. On macOS, it checks for Apple developer tools.

Installation delegates to an available system package manager using predefined commands.
DevSetup does not bundle compilers or download and execute arbitrary installation scripts.
A successful command can mean that an installer was launched; wait for that installer to finish.
Detection checks for files, not a complete working SDK. Building a project verifies the toolchain.

## Supported environments

| Platform | Detection | Compiler installation |
| --- | --- | --- |
| Windows | MSVC via vswhere; GCC/Clang on PATH | MSVC Build Tools 2022 + SDK via winget |
| Linux | GCC/Clang on PATH | GCC/Clang via apt, dnf, pacman, or zypper |
| macOS | Apple developer tools and GCC/Clang on PATH | Xcode Command Line Tools |

VS Code installation uses winget on Windows, Snap on Linux, or an existing Homebrew installation on macOS.
If a supported package manager is unavailable, the utility prints an official installer link.
Standalone LLVM installation is not offered on Windows because a complete toolchain also needs an SDK.
The MSVC configuration targets x64; ARM builds need separate validation and target adaptation.
Android is recognized, but installation on Android and iOS is not supported.

## Generated VS Code files

Inside the selected project's `.vscode` directory, DevSetup creates:

- `tasks.json`: a default build task for the active C++17 source file.
- `c_cpp_properties.json`: the compiler path, language standard, and project include path for IntelliSense.
- `extensions.json`: a recommendation for Microsoft's C/C++ extension, if this file does not already exist.
- `devsetup-build.ps1` (MSVC only): loads the Visual Studio environment and invokes the compiler.

The executable is written beside the source file. Multi-file projects need CMake or a custom build task.
A debugger configuration (`launch.json`) is not generated.

The MSVC task uses `ExecutionPolicy Bypass` only for its own process; it does not change system policy.
Conflicting task/property/helper files are replaced only after confirmation, with numbered `.bak` backups.
Declining any conflict cancels configuration before files are changed.
An existing `extensions.json` is preserved.

## Building from source

Windows with PowerShell and MSVC Build Tools including the C++ workload:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./build-windows.ps1
```

Linux/macOS with a C++17 compiler and CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/devsetup
```

Without CMake: `c++ -std=c++17 -O2 src/main.cpp -o devsetup`.

To reproduce the static Linux archive and `.deb` release on Linux x86_64, install
GCC with static standard libraries, Python 3, tar, and dpkg-deb, then run:

```sh
bash build-linux.sh
```

On Ubuntu these build tools are available through `sudo apt install build-essential python3 dpkg-dev`.
The script runs `test-linux.py` before packaging; it does not install the generated package.

Build separately for each operating system and architecture. Linux binaries also depend on
the target libc/libstdc++ versions. A Windows `.exe` is not a universal binary for Linux, macOS, or phones.

## Command-line options and verification

```powershell
./dist/devsetup-windows-x64.exe --check
./dist/devsetup-windows-x64.exe --configure "C:/Projects/Hello"
./dist/devsetup-windows-x64.exe --help
powershell -NoProfile -ExecutionPolicy Bypass -File ./test-windows.ps1
```

`--configure DIRECTORY` uses the first detected compiler, installs no packages, and refuses
to overwrite conflicting files. Without arguments, DevSetup opens its interactive menu.

The Windows smoke test checks detection, JSON, actual compilation and execution through
the generated task, repeat configuration, conflict protection, and input handling.
Test artifacts remain in `test-output`.

Windows and Linux were verified locally with actual compilation and execution through
their generated VS Code tasks. Linux verification used Ubuntu 26.04.1 LTS in WSL2.
Compiler/editor installation commands and macOS execution have not been exercised.

## Platform documentation

- [MSVC in VS Code](https://code.visualstudio.com/docs/cpp/config-msvc/)
- [IntelliSense settings](https://code.visualstudio.com/docs/cpp/customize-cpp-settings/)
- [VS Code on Linux](https://code.visualstudio.com/docs/setup/linux)
- [Visual Studio installation parameters](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio?view=vs-2022)
- [Apple Command Line Tools](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools/)
- [VS Code in Homebrew](https://formulae.brew.sh/cask/visual-studio-code)

## Windows: compiler found but installation unavailable

If MSVC is listed as installed, use menu option 4 to configure your project; you do not need to reinstall it.
Automatic installation uses winget. DevSetup recognizes Windows App Execution Aliases and checks
`%LOCALAPPDATA%/Microsoft/WindowsApps` even when that directory is missing from PATH.
If winget is genuinely missing, install or update [App Installer](https://apps.microsoft.com/detail/9nblggh4nns1),
then restart DevSetup. You can still configure an existing compiler without winget.

## Functional verification

See the [verification report](devsetup/TESTING.md) for tested features, reproduction commands, and remaining platform/installation limits.
