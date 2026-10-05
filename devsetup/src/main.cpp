#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace fs = std::filesystem;
using std::string;
struct Compiler { string name; fs::path path; fs::path vcvars; };
string os() {
#ifdef _WIN32
 return "Windows";
#elif defined(__ANDROID__)
 return "Android";
#elif defined(__APPLE__)
 return "macOS";
#elif defined(__linux__)
 return "Linux";
#else
 return "Unknown";
#endif
}
string arch() {
#if defined(_M_ARM64) || defined(__aarch64__)
 return "arm64";
#elif defined(_M_X64) || defined(__x86_64__)
 return "x64";
#else
 return "x86";
#endif
}
string env(const char* key) {
#ifdef _WIN32
 auto wkey = fs::u8path(key).wstring();
 DWORD n = GetEnvironmentVariableW(wkey.c_str(), nullptr, 0);
 if (!n) return {};
 std::wstring value(n, L'\0');
 GetEnvironmentVariableW(wkey.c_str(), value.data(), n);
 value.resize(n - 1); return fs::path(value).u8string();
#else
 const char* p = std::getenv(key); return p ? p : "";
#endif
}
fs::path findExe(const string& name) {
 string paths = env("PATH");
#ifdef _WIN32
 const char separator = ';';
 const std::vector<string> extensions = {".exe", ".cmd", ".bat", ""};
#else
 const char separator = ':';
 const std::vector<string> extensions = {""};
#endif
 std::istringstream in(paths); string part;
 while (std::getline(in, part, separator)) {
  if (part.empty()) continue;
  if (part.front() == '"' && part.back() == '"') part = part.substr(1, part.size()-2);
  for (const auto& ext : extensions) {
   auto p = fs::u8path(part) / (name + ext);
   std::error_code ec;
   if (!fs::is_regular_file(p, ec)) continue;
#ifndef _WIN32
   if (access(p.c_str(), X_OK) != 0) continue;
#endif
   return fs::absolute(p);
  }
 }
 return {};
}
// Only fixed, reviewed commands are passed to this shell helper.
string capture(const string& command) {
#ifdef _WIN32
 FILE* pipe = _wpopen(fs::u8path(command).wstring().c_str(), L"rt");
#else
 FILE* pipe = popen(command.c_str(), "r");
#endif
 if (!pipe) return {};
 string result; char buffer[4096];
 while (std::fgets(buffer, sizeof(buffer), pipe)) result += buffer;
#ifdef _WIN32
 _pclose(pipe);
#else
 pclose(pipe);
#endif
 while (!result.empty() && (result.back()=='\r' || result.back()=='\n')) result.pop_back();
 return result;
}
string json(const string& value) {
 string out = "\"";
 const char* hex = "0123456789abcdef";
 for (unsigned char c : value) {
  if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
  else if (c < 32) { out += "\\u00"; out += hex[c>>4]; out += hex[c&15]; }
  else out += char(c);
 }
 return out + '"';
}
string psQuote(string s) {
 string out = "'"; for (char c : s) { out += c; if (c=='\'') out += c; } return out + "'";
}
std::vector<Compiler> compilers() {
 std::vector<Compiler> out;
#ifdef _WIN32
 auto where = fs::u8path(env("ProgramFiles(x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe";
 if (fs::exists(where) && where.u8string().find_first_of("\"%\r\n") == string::npos) {
  string root = capture("\"\"" + where.u8string() + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -utf8\"");
  if (!root.empty()) {
   auto base = fs::u8path(root); string version;
   std::ifstream ver(base / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt");
   std::getline(ver, version); if (!version.empty() && version.back()=='\r') version.pop_back();
   auto cl = base / "VC/Tools/MSVC" / version / "bin/Hostx64/x64/cl.exe";
   auto vars = base / "VC/Auxiliary/Build/vcvars64.bat";
   if (fs::exists(cl) && fs::exists(vars)) out.push_back({"MSVC", cl, vars});
  }
 }
#elif defined(__APPLE__)
 if (capture("xcode-select -p 2>/dev/null").empty()) return out;
#endif
 for (auto name : {"g++", "clang++"}) {
  auto p = findExe(name); if (!p.empty()) out.push_back({name, p, {}});
 }
 return out;
}
fs::path codePath() {
 auto p = findExe("code"); if (!p.empty()) return p;
#ifdef _WIN32
 for (auto root : {fs::u8path(env("LOCALAPPDATA"))/"Programs", fs::u8path(env("ProgramFiles"))}) {
  p = root / "Microsoft VS Code/bin/code.cmd"; if (fs::exists(p)) return p;
 }
#elif defined(__APPLE__)
 p = "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code";
 if (fs::exists(p)) return p;
#endif
 return {};
}
string ask(const string& prompt) {
 std::cout << prompt << std::flush; string line;
 if (!std::getline(std::cin, line)) throw std::runtime_error("End of input.");
 return line;
}
bool yes(const string& prompt) { auto s = ask(prompt + " [y/N]: "); return s=="y" || s=="Y" || s=="yes" || s=="YES"; }
int command(const string& cmd) {
 std::cout << "\n$ " << cmd << "\n";
 if (!yes("Run this command? Administrator credentials may be required")) return -1;
#ifdef _WIN32
 int status = _wsystem(fs::u8path(cmd).wstring().c_str());
#else
 int status = std::system(cmd.c_str());
#endif
#ifndef _WIN32
 if (status != -1 && WIFEXITED(status)) status = WEXITSTATUS(status);
#endif
 std::cout << "Exit code: " << status << "\n";
 return status;
}
struct Option { string label, cmd; };
void chooseInstall(bool editor) {
 std::vector<Option> options;
 if (os()=="Windows" && !findExe("winget").empty()) {
  if (editor) options.push_back({"Visual Studio Code (winget)", "winget install --exact --id Microsoft.VisualStudioCode"});
  else options.push_back({"MSVC + Windows SDK (Build Tools 2022)", "winget install --exact --id Microsoft.VisualStudio.2022.BuildTools --override \"--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended\""});
 } else if (os()=="macOS") {
  if (!editor) options.push_back({"Apple Clang / Xcode Command Line Tools", "xcode-select --install"});
  if (editor && !findExe("brew").empty()) options.push_back({"Visual Studio Code (Homebrew)", "brew install --cask visual-studio-code"});
 } else if (os()=="Linux") {
  string prefix;
#ifndef _WIN32
  if (geteuid()!=0) { if (findExe("sudo").empty()) { std::cout << "sudo is unavailable. Contact your administrator.\n"; return; } prefix="sudo "; }
#endif
  if (editor) {
   if (!findExe("snap").empty()) options.push_back({"Visual Studio Code (Snap)", prefix+"snap install code --classic"});
  } else if (!findExe("apt-get").empty()) {
   options.push_back({"GCC + GDB (apt)", prefix+"apt-get update && "+prefix+"apt-get install build-essential gdb"});
   options.push_back({"Clang + GDB (apt)", prefix+"apt-get update && "+prefix+"apt-get install clang build-essential gdb"});
  } else if (!findExe("dnf").empty()) {
   options.push_back({"GCC + GDB (dnf)", prefix+"dnf install gcc-c++ gdb"});
   options.push_back({"Clang + GDB (dnf)", prefix+"dnf install clang gcc-c++ gdb"});
  } else if (!findExe("pacman").empty()) {
   options.push_back({"GCC + GDB (pacman, full system upgrade)", prefix+"pacman -Syu --needed gcc gdb"});
   options.push_back({"Clang + GDB (pacman, full system upgrade)", prefix+"pacman -Syu --needed clang gcc gdb"});
  } else if (!findExe("zypper").empty()) {
   options.push_back({"GCC + GDB (zypper)", prefix+"zypper install gcc-c++ gdb"});
   options.push_back({"Clang + GDB (zypper)", prefix+"zypper install clang gcc-c++ gdb"});
  }
 }
 if (options.empty()) {
  std::cout << "Automatic installation is unavailable in this environment.\n";
  if (editor) std::cout << "Download VS Code: https://code.visualstudio.com/download\n";
  else if (os()=="Windows") std::cout << "Install App Installer (winget) or Build Tools: https://visualstudio.microsoft.com/downloads/\n";
  return;
 }
 for (size_t i=0; i<options.size(); ++i) std::cout << i+1 << ") " << options[i].label << '\n';
 auto s = ask("Select an option (0 to cancel): ");
 for (size_t i=0; i<options.size(); ++i) if (s==std::to_string(i+1)) {
  if (command(options[i].cmd)==0) std::cout << "Request completed. Wait for the installer to finish; restart this utility if PATH needs to be refreshed.\n";
  return;
 }
}
void report() {
 std::cout << "\nOS: " << os() << " | build architecture: " << arch() << '\n';
 auto list = compilers();
 if (list.empty()) std::cout << "[--] No compiler found. Select installation from the menu.\n";
 for (const auto& c : list) std::cout << "[OK] " << c.name << ": " << c.path.u8string() << '\n';
 auto code = codePath(); std::cout << (code.empty() ? "[--] VS Code not found" : "[OK] VS Code: "+code.u8string()) << '\n';
 for (auto name : {"winget", "apt-get", "dnf", "pacman", "zypper", "brew", "snap"}) {
  auto p = findExe(name); if (!p.empty()) std::cout << "[OK] Package manager: " << name << '\n';
 }
 std::cout << "Finding compiler files does not verify the SDK. Build a project to check the toolchain.\n";
}
string read(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
void write(const fs::path& p, const string& content) {
 std::ofstream f(p, std::ios::binary); f << content; f.close();
 if (!f) throw std::runtime_error("Failed to write: "+p.u8string());
}
void configure(const fs::path& folder, const Compiler& c, bool interactive) {
 auto root = fs::absolute(folder); auto dir = root / ".vscode";
 std::vector<std::pair<string,string>> files;
 string task;
 const bool msvc = c.name=="MSVC";
 if (msvc) {
  string vars = c.vcvars.u8string();
  if (vars.find_first_of("%\"\r\n")!=string::npos) throw std::runtime_error("Unsupported characters in the Visual Studio path.");
  string script = "\xEF\xBB\xBF";
  script += "param([Parameter(Mandatory=$true)][string]$Source, [Parameter(Mandatory=$true)][string]$Output)\r\n$ErrorActionPreference = 'Stop'\r\n";
  script += "$vars = "+psQuote(vars)+"\r\n$lines = & $env:ComSpec /d /c ('call \"' + $vars + '\" >nul && set')\r\nif ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\r\nforeach ($line in $lines) {\r\n if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }\r\n}\r\n& cl.exe /nologo /EHsc /std:c++17 /utf-8 /Zi $Source ('/Fe' + $Output) ('/Fo' + [IO.Path]::ChangeExtension($Output, '.obj')) ('/Fd' + [IO.Path]::ChangeExtension($Output, '.pdb'))\r\nexit $LASTEXITCODE\r\n";
  files.push_back({"devsetup-build.ps1", script});
  task = "\"command\": \"powershell.exe\", \"args\": [\"-NoProfile\", \"-ExecutionPolicy\", \"Bypass\", \"-File\", \"${workspaceFolder}/.vscode/devsetup-build.ps1\", \"-Source\", \"${file}\", \"-Output\", \"${fileDirname}/${fileBasenameNoExtension}.exe\"]";
 } else {
  task = "\"command\": "+json(c.path.u8string())+", \"args\": [\"-std=c++17\", \"-g\", \"-Wall\", \"-Wextra\", \"${file}\", \"-o\", \"${fileDirname}/${fileBasenameNoExtension}"+(os()=="Windows" ? ".exe" : "")+"\"]";
 }
 files.push_back({"tasks.json", "{\n  \"version\": \"2.0.0\",\n  \"tasks\": [{\n    \"label\": \"C++: build active file\", \"type\": \"process\",\n    "+task+",\n    \"options\": {\"cwd\": \"${fileDirname}\"},\n    \"problemMatcher\": ["+json(msvc ? "$msCompile" : "$gcc")+"],\n    \"group\": {\"kind\": \"build\", \"isDefault\": true}\n  }]\n}\n"});
 files.push_back({"c_cpp_properties.json", "{\n  \"version\": 4,\n  \"configurations\": [{\n    \"name\": "+json(os())+",\n    \"compilerPath\": "+json(c.path.u8string())+",\n    \"cppStandard\": \"c++17\",\n    \"includePath\": [\"${workspaceFolder}/**\"]\n  }]\n}\n"});
 // Check every conflict before modifying anything. Existing extension recommendations are kept.
 for (const auto& file : files) {
  auto p = dir/file.first;
  if (fs::exists(p) && read(p)!=file.second) {
   if (!interactive || !yes("Replace "+p.u8string()+" and create a backup?")) throw std::runtime_error("Setup cancelled: existing files were preserved.");
  }
 }
 fs::create_directories(dir);
 for (const auto& file : files) {
  auto p = dir/file.first;
  if (fs::exists(p)) {
   if (read(p)==file.second) continue;
   auto backup=p; backup += ".bak";
   for (int i=1; fs::exists(backup); ++i) { backup=p; backup += ".bak."+std::to_string(i); }
   fs::copy_file(p, backup);
   std::cout << "Backup: " << backup.u8string() << '\n';
  }
  write(p, file.second);
 }
 if (!fs::exists(dir/"extensions.json")) write(dir/"extensions.json", "{\"recommendations\": [\"ms-vscode.cpptools\"]}\n");
 std::cout << "[OK] Configured: " << root.u8string() << "\nOpen this folder in VS Code and install the recommended C/C++ extension.\nOpen a .cpp file and press Ctrl+Shift+B (macOS: Cmd+Shift+B).\nBuilds the active file using C++17. Run the executable from the terminal.\n";
}
void setup() {
 auto list = compilers();
 if (list.empty()) { std::cout << "Install a compiler first.\n"; chooseInstall(false); list=compilers(); if (list.empty()) return; }
 for (size_t i=0; i<list.size(); ++i) std::cout << i+1 << ") " << list[i].name << " - " << list[i].path.u8string() << '\n';
 auto s=ask("Select a compiler (0 to cancel): ");
 for (size_t i=0; i<list.size(); ++i) if (s==std::to_string(i+1)) {
  auto path=ask("Project folder (Enter for current folder; no quotes): ");
  configure(path.empty()?fs::current_path():fs::u8path(path), list[i], true);
  if (codePath().empty()) { std::cout << "VS Code has not been found yet.\n"; chooseInstall(true); }
  return;
 }
}
void installExtension() {
 auto code=codePath();
 if (code.empty()) { std::cout << "Install VS Code first.\n"; return; }
 string path=code.u8string();
#ifdef _WIN32
 if (path.find_first_of("\"%!\r\n")!=string::npos) throw std::runtime_error("Install ms-vscode.cpptools from the Extensions panel in VS Code.");
 command("\"\""+path+"\" --install-extension ms-vscode.cpptools\"");
#else
 string quoted="'"; for (char c:path) { if(c=='\'') quoted+="'\\''"; else quoted+=c; } quoted+="'";
 command(quoted+" --install-extension ms-vscode.cpptools");
#endif
}
int app(const std::vector<string>& args) {
 if (args.size()>1) {
  if (args[1]=="--check" && args.size()==2) { report(); return 0; }
  if (args[1]=="--configure" && args.size()==3) {
   auto list=compilers(); if (list.empty()) throw std::runtime_error("No compiler found.");
   configure(fs::u8path(args[2]), list.front(), false); return 0;
  }
  if (args[1]=="--help") { std::cout << "devsetup [--check | --configure DIRECTORY | --help]\nNo arguments: interactive menu. --configure: use the first detected compiler without overwriting conflicting files.\n"; return 0; }
  throw std::runtime_error("Unknown arguments. Use --help.");
 }
 std::cout << "\n  DEVSETUP / C++\n  Console environment setup\n"; report();
 while (true) {
  std::cout << "\n  1  Check environment\n  2  Install a compiler\n  3  Install VS Code\n  4  Configure a project for VS Code\n  5  Install the C/C++ extension\n  0  Exit\n\n";
  auto choice=ask("> "); if (choice=="0") return 0;
  try {
   if (choice=="1") report(); else if (choice=="2") chooseInstall(false);
   else if (choice=="3") chooseInstall(true); else if (choice=="4") setup(); else if (choice=="5") installExtension();
   else std::cout << "Enter a menu option number.\n";
  } catch (const std::exception& e) { std::cerr << "[!] " << e.what() << '\n'; if (!std::cin) return 1; }
 }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
 SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
 std::vector<string> args; for(int i=0;i<argc;++i) args.push_back(fs::path(argv[i]).u8string());
#else
int main(int argc, char** argv) {
 std::vector<string> args(argv, argv+argc);
#endif
 try { return app(args); } catch (const std::exception& e) { std::cerr << "[!] " << e.what() << '\n'; return 1; }
}
