/*

 MIT License

 Copyright © 2021-2024 Samuel Venable
 Copyright © 2021-2024 devKathy

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <thread>
#include <mutex>

#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cstdio>

#include "process.hpp"

#if !defined(_WIN32)
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

#if defined(_WIN32)
#include <shlwapi.h>
#include <objbase.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <psapi.h>
#include <io.h>
#elif (defined(__APPLE__) && defined(__MACH__))
#include <sys/sysctl.h>
#include <libproc.h>
#elif (defined(__linux__) || defined(__ANDROID__))
#include <dirent.h>
#elif (defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__))
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <kvm.h>
#elif defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <kvm.h>
#elif defined(__sun)
#include <kvm.h>
#include <dirent.h>
#include <libproc.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/param.h>
#include <sys/procfs.h>
#endif
#include <sys/stat.h>
#if (defined(USE_SDL_POLLEVENT) || defined(USE_SDL2_POLLEVENT))
#include <SDL.h>
#endif
#if defined(USE_SDL3_POLLEVENT)
#include <SDL3/SDL.h>
#endif
#if (defined(_WIN32) && defined(_MSC_VER))
#pragma comment(lib, "ntdll.lib")
#if (defined(USE_SDL_POLLEVENT) || defined(USE_SDL2_POLLEVENT))
#pragma comment(lib, "SDL2.lib")
#endif
#if defined(USE_SDL3_POLLEVENT)
#pragma comment(lib, "SDL3.lib")
#endif
#endif

namespace {

  void message_pump() {
    #if defined(_WIN32)
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    #endif
  }

  std::vector<std::string> string_split_by_first_equals_sign(std::string str) {
    std::size_t pos = 0;
    std::vector<std::string> vec;
    if ((pos = str.find_first_of("=")) != std::string::npos) {
      vec.push_back(str.substr(0, pos));
      vec.push_back(str.substr(pos + 1));
    }
    return vec;
  }

  #if defined(_WIN32)
  enum MEMTYP {
    MEMCMD,
    MEMENV,
    MEMCWD
  };

  #if !defined(_MSC_VER)
  #pragma pack(push, 8)
  #else
  #include <pshpack8.h>
  #endif

  /* CURDIR struct from:
   https://github.com/processhacker/phnt/
   CC BY 4.0 licence */

  typedef struct {
    UNICODE_STRING DosPath;
    HANDLE Handle;
  } CURDIR;

  /* RTL_DRIVE_LETTER_CURDIR struct from:
   https://github.com/processhacker/phnt/
   CC BY 4.0 licence */

  typedef struct {
    USHORT Flags;
    USHORT Length;
    ULONG TimeStamp;
    STRING DosPath;
  } RTL_DRIVE_LETTER_CURDIR;

  /* RTL_USER_PROCESS_PARAMETERS struct from:
   https://github.com/processhacker/phnt/
   CC BY 4.0 licence */

  typedef struct {
    ULONG MaximumLength;
    ULONG Length;
    ULONG Flags;
    ULONG DebugFlags;
    HANDLE ConsoleHandle;
    ULONG ConsoleFlags;
    HANDLE StandardInput;
    HANDLE StandardOutput;
    HANDLE StandardError;
    CURDIR CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    PVOID Environment;
    ULONG StartingX;
    ULONG StartingY;
    ULONG CountX;
    ULONG CountY;
    ULONG CountCharsX;
    ULONG CountCharsY;
    ULONG FillAttribute;
    ULONG WindowFlags;
    ULONG ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopInfo;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
    RTL_DRIVE_LETTER_CURDIR CurrentDirectories[32];
    ULONG_PTR EnvironmentSize;
    ULONG_PTR EnvironmentVersion;
    PVOID PackageDependencyData;
    ULONG ProcessGroupId;
    ULONG LoaderThreads;
    UNICODE_STRING RedirectionDllName;
    UNICODE_STRING HeapPartitionName;
    ULONG_PTR DefaultThreadpoolCpuSetMasks;
    ULONG DefaultThreadpoolCpuSetMaskCount;
  } RTL_USER_PROCESS_PARAMETERS;

  #if !defined(_MSC_VER)
  #pragma pack(pop)
  #else
  #include <poppack.h>
  #endif

  std::wstring widen(std::string str) {
    if (str.empty()) return L"";
    std::size_t wchar_count = str.size() + 1;
    std::vector<wchar_t> buf(wchar_count);
    return std::wstring{ buf.data(), (std::size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), (int)wchar_count) };
  }

  std::string narrow(std::wstring wstr) {
    if (wstr.empty()) return "";
    int nbytes = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
    std::vector<char> buf(nbytes);
    return std::string { buf.data(), (std::size_t)WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), buf.data(), nbytes, nullptr, nullptr) };
  }

  HANDLE open_process_with_debug_privilege(ngs::ps::NGS_PROCID proc_id) {
    HANDLE proc = nullptr;
    HANDLE hToken = nullptr;
    LUID luid;
    TOKEN_PRIVILEGES tkp;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
      if (LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Luid = luid;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (AdjustTokenPrivileges(hToken, false, &tkp, sizeof(tkp), nullptr, nullptr)) {
          proc = OpenProcess(PROCESS_ALL_ACCESS, false, proc_id);
        }
      }
      CloseHandle(hToken);
    }
    if (!proc) {
      proc = OpenProcess(PROCESS_ALL_ACCESS, false, proc_id);
    }
    return proc;
  }

  std::vector<wchar_t> cmd_env_cwd_from_proc(HANDLE proc, int type) {
    std::vector<wchar_t> buffer;
    PEB peb;
    SIZE_T nRead = 0;
    ULONG len = 0;
    PVOID buf = nullptr;
    PROCESS_BASIC_INFORMATION pbi;
    RTL_USER_PROCESS_PARAMETERS upp;
    NTSTATUS status = NtQueryInformationProcess(proc, ProcessBasicInformation, &pbi, sizeof(pbi), &len);
    ULONG error = RtlNtStatusToDosError(status);
    if (error) return buffer;
    ReadProcessMemory(proc, pbi.PebBaseAddress, &peb, sizeof(peb), &nRead);
    if (!nRead) return buffer;
    ReadProcessMemory(proc, peb.ProcessParameters, &upp, sizeof(upp), &nRead);
    if (!nRead) return buffer;
    if (type == MEMCMD) {
      buf = upp.CommandLine.Buffer;
      len = upp.CommandLine.Length;
    } else if (type == MEMENV) {
      buf = upp.Environment;
      len = (ULONG)upp.EnvironmentSize;
    } else {
      buf = upp.CurrentDirectory.DosPath.Buffer;
      len = upp.CurrentDirectory.DosPath.Length;
    }
    buffer.resize(len / 2 + 1);
    ReadProcessMemory(proc, buf, &buffer[0], len, &nRead);
    if (!nRead) return buffer;
    buffer[len / 2] = L'\0';
    return buffer;
  }
  #elif (defined(__APPLE__) && defined(__MACH__))
  enum MEMTYP {
    MEMCMD,
    MEMENV
  };

  std::vector<std::string> cmd_env_from_proc_id(ngs::ps::NGS_PROCID proc_id, int type) {
    std::vector<std::string> vec;
    std::size_t len = 0;
    int argmax = 0, nargs = 0;
    char *procargs = nullptr, *sp = nullptr, *cp = nullptr;
    int mib[3];
    mib[0] = CTL_KERN;
    mib[1] = KERN_ARGMAX;
    len = sizeof(argmax);
    if (sysctl(mib, 2, &argmax, &len, nullptr, 0)) {
      return vec;
    }
    procargs = (char *)malloc(argmax);
    if (!procargs) {
      return vec;
    }
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS2;
    mib[2] = proc_id;
    len = argmax;
    if (sysctl(mib, 3, procargs, &len, nullptr, 0)) {
      free(procargs);
      return vec;
    }
    memcpy(&nargs, procargs, sizeof(nargs));
    cp = procargs + sizeof(nargs);
    for (; cp < &procargs[len]; cp++) {
      if (*cp == '\0') break;
    }
    if (cp == &procargs[len]) {
      free(procargs);
      return vec;
    }
    for (; cp < &procargs[len]; cp++) {
      if (*cp != '\0') break;
    }
    if (cp == &procargs[len]) {
      free(procargs);
      return vec;
    }
    sp = cp;
    int i = 0;
    while ((*sp != '\0' || i < nargs) && sp < &procargs[len]) {
      if (type && i >= nargs) {
        vec.push_back(sp);
      } else if (!type && i < nargs) {
        vec.push_back(sp);
      }
      sp += strlen(sp) + 1;
      i++;
    }
    free(procargs);
    return vec;
  }
  #elif defined(__sun)
  enum MEMTYP {
    MEMCMD,
    MEMENV
  };

  std::vector<std::string> cmd_env_from_proc_id(ngs::ps::NGS_PROCID proc_id, int type) {
    std::vector<std::string> vec;
    auto proc_psinfo_get = [](psinfo_t *psinfo, ngs::ps::NGS_PROCID proc_id) {
      int fd = -1, retval = 0;
      char buffer[BUFSIZ];
      sprintf(buffer, "/proc/%d/psinfo", proc_id);
      if ((fd = open(buffer, O_RDONLY)) < 0) {
        return ESRCH;
      }
      if (pread(fd, psinfo, sizeof(psinfo_t), 0) != sizeof(psinfo_t)) {
        retval = errno;
      }
      close(fd);
      return retval;
    };
    psinfo_t psinfo;
    char buffer[BUFSIZ];
    int n = 0, err = 0, fd = -1;
    std::size_t nread = 0;
    unsigned args_size = 0;
    char **args = (char **)malloc(ARG_MAX);
    if (!args) goto finish;
    psinfo.pr_dmodel = 0;
    err = proc_psinfo_get(&psinfo, proc_id);
    if (err) {
      free(args);
      goto finish;
    }
    args_size = sizeof(*args) * ARG_MAX;
    sprintf(buffer, "/proc/%d/as", proc_id);
    if ((fd = open(buffer, O_RDONLY)) < 0) {
      free(args);
      goto finish;
    }
    if (args_size > sizeof(args)) {
      free(args);
      args = (char **)malloc(args_size);
      if (!args) goto finish;
    }
    if ((long)(nread = pread(fd, args, args_size, (off_t)((type != MEMCMD) ? psinfo.pr_envp : psinfo.pr_argv))) <= 0) {
      close(fd);
    }
    for (n = 0; args[n]; n++) {
      int len = 0;
      char *arg = nullptr;
      if ((long)(nread = pread(fd, buffer, sizeof(buffer), (off_t)args[n])) <= 0) {
        close(fd);
        break;
      }
      len = strlen(buffer) + 1;
      arg = (char *)malloc(len);
      if (!arg) {
        if (args) free(args);
        vec.clear();
        goto finish;
      }
      memcpy(arg, buffer, len);
      vec.push_back(arg);
    }
    if (args) {
      free(args);
    }
    finish:
    if (fd != -1) {
      close(fd);
    }
    return vec;
  }
  #endif

} // anonymous namespace

namespace ngs::ps {

  NGS_PROCID proc_id_from_self() {
    #if !defined(_WIN32)
    return getpid();
    #else
    return GetCurrentProcessId();
    #endif
  }

  std::vector<NGS_PROCID> proc_id_enum() {
    std::vector<NGS_PROCID> vec;
    #if defined(_WIN32)
    HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!hp) return vec;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hp, &pe)) {
      do {
        message_pump();
        vec.push_back(pe.th32ProcessID);
      } while (Process32Next(hp, &pe));
    }
    CloseHandle(hp);
    #elif (defined(__APPLE__) && defined(__MACH__))
    vec.push_back(0);
    std::vector<NGS_PROCID> proc_info;
    proc_info.resize(proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0));
    int cntp = proc_listpids(PROC_ALL_PIDS, 0, &proc_info[0], sizeof(NGS_PROCID) * proc_info.size());
    for (int i = cntp - 1; i >= 0; i--) {
      if (proc_info[i] <= 0) continue;
      vec.push_back(proc_info[i]);
    }
    #elif (defined(__linux__) || defined(__ANDROID__) || defined(__sun))
    vec.push_back(0);
    DIR *proc = opendir("/proc");
    struct dirent *ent = nullptr;
    NGS_PROCID tgid = 0;
    if (proc == nullptr) return vec;
    while ((ent = readdir(proc))) {
      if (!isdigit(*ent->d_name))
        continue;
      tgid = strtoul(ent->d_name, nullptr, 10);
      vec.push_back(tgid);
    }
    closedir(proc);
    #elif defined(__FreeBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PROC, 0, &cntp))) {
      for (int i = 0; i < cntp; i++) {
        vec.push_back(proc_info[i].ki_pid);
      }
    }
    kvm_close(kd);
    #elif defined(__DragonFly__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_ALL, 0, &cntp))) {
      for (int i = 0; i < cntp; i++) {
        if (proc_info[i].kp_pid >= 0) {
          vec.push_back(proc_info[i].kp_pid);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__NetBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc2 *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc2), &cntp))) {
      for (int i = cntp - 1; i >= 0; i--) {
        vec.push_back(proc_info[i].p_pid);
      }
    }
    kvm_close(kd);
    #elif defined(__OpenBSD__)
    vec.push_back(0);
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc), &cntp))) {
      for (int i = cntp - 1; i >= 0; i--) {
        vec.push_back(proc_info[i].p_pid);
      }
    }
    kvm_close(kd);
    #endif
    #if defined(__sun)
    struct pid cur_pid;
    kvm_t *kd = nullptr;
    struct proc *proc_info = nullptr;
    if (!vec.empty()) { 
      goto finish;
    }
    kd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    while ((proc_info = kvm_nextproc(kd))) {
      if (kvm_kread(kd, (std::uintptr_t)proc_info->p_pidp, &cur_pid, sizeof(cur_pid)) != -1) {
        vec.insert(vec.begin(), cur_pid.pid_id);
      }
    }
    kvm_close(kd);
    finish:
    #endif
    std::sort(vec.begin(), vec.end());
    auto itr = std::unique(vec.begin(), vec.end());
    vec.erase(itr, vec.end());
    if (vec.size() == 1 && vec[0] == 0) {
      vec.clear();
    }
    return vec;
  }

  bool proc_id_exists(NGS_PROCID proc_id) {
    if (proc_id < 0) return false;
    std::vector<NGS_PROCID> vec;
    vec = proc_id_enum();
    auto itr = std::find(vec.begin(), vec.end(), proc_id);
    return (itr != vec.end());
  }

  bool proc_id_suspend(NGS_PROCID proc_id) {
    if (proc_id < 0) return false;
    #if !defined(_WIN32)
    return (!kill(proc_id, SIGSTOP));
    #else
    HANDLE proc = open_process_with_debug_privilege(proc_id);
    if (proc == nullptr) return false;
    typedef NTSTATUS (__stdcall *NTSP)(IN HANDLE ProcessHandle);
    HMODULE hModule = GetModuleHandleW(L"ntdll.dll");
    if (!hModule) return false;
    FARPROC farProc = GetProcAddress(hModule, "NtSuspendProcess");
    if (!farProc) return false;
    NTSP NtSuspendProcess = (NTSP)farProc;
    NTSTATUS status = NtSuspendProcess(proc);
    ULONG error = RtlNtStatusToDosError(status);
    CloseHandle(proc);
    return (!error);
    #endif
  }

  bool proc_id_resume(NGS_PROCID proc_id) {
    if (proc_id < 0) return false;
    #if !defined(_WIN32)
    return (!kill(proc_id, SIGCONT));
    #else
    HANDLE proc = open_process_with_debug_privilege(proc_id);
    if (proc == nullptr) return false;
    typedef NTSTATUS (__stdcall *NTRP)(IN HANDLE ProcessHandle);
    HMODULE hModule = GetModuleHandleW(L"ntdll.dll");
    if (!hModule) return false;
    FARPROC farProc = GetProcAddress(hModule, "NtResumeProcess");
    if (!farProc) return false;
    NTRP NtResumeProcess = (NTRP)farProc;
    NTSTATUS status = NtResumeProcess(proc);
    ULONG error = RtlNtStatusToDosError(status);
    CloseHandle(proc);
    return (!error);
    #endif
  }

  bool proc_id_kill(NGS_PROCID proc_id) {
    if (proc_id < 0) return false;
    #if !defined(_WIN32)
    return (!kill(proc_id, SIGKILL));
    #else
    HANDLE proc = open_process_with_debug_privilege(proc_id);
    if (proc == nullptr) return false;
    bool result = TerminateProcess(proc, 0);
    CloseHandle(proc);
    return result;
    #endif
  }

  std::vector<NGS_PROCID> parent_proc_id_from_proc_id(NGS_PROCID proc_id) {
    std::vector<NGS_PROCID> vec;
    if (proc_id < 0) return vec;
    #if defined(_WIN32)
    HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!hp) return vec;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hp, &pe)) {
      do {
        if (pe.th32ProcessID == proc_id) {
          message_pump();
          vec.push_back(pe.th32ParentProcessID);
          break;
        }
      } while (Process32Next(hp, &pe));
    }
    CloseHandle(hp);
    #elif (defined(__APPLE__) && defined(__MACH__))
    proc_bsdinfo proc_info;
    if (proc_pidinfo(proc_id, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) > 0) {
      vec.push_back(proc_info.pbi_ppid);
    }
    if (vec.empty() && (proc_id == 0 || proc_id == 1))
      vec.push_back(0);
    #elif (defined(__linux__) || defined(__ANDROID__))
    char buffer[BUFSIZ];
    sprintf(buffer, "/proc/%d/stat", proc_id);
    FILE *stat = fopen(buffer, "r");
    if (stat) {
      std::size_t size = fread(buffer, sizeof(char), sizeof(buffer), stat);
      if (size > 0) {
        char *token = nullptr;
        if (((token = strtok(buffer, " "))) &&
          ((token = strtok(nullptr, " "))) &&
          ((token = strtok(nullptr, " "))) &&
          ((token = strtok(nullptr, " ")))) {
          NGS_PROCID parent_proc_id = strtoul(token, nullptr, 10);
          vec.push_back(parent_proc_id);
        }
      }
      fclose(stat);
    }
    if (vec.empty() && proc_id == 0)
      vec.push_back(0);
    #elif defined(__FreeBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, &cntp))) {
      vec.push_back(proc_info->ki_ppid);
    }
    kvm_close(kd);
    #elif defined(__DragonFly__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, &cntp))) {
      if (proc_info->kp_ppid >= 0) {
        vec.push_back(proc_info->kp_ppid);
      }
    }
    kvm_close(kd);
    if (vec.empty() && proc_id == 0)
      vec.push_back(0);
    #elif defined(__NetBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc2 *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc2(kd, KERN_PROC_PID, proc_id, sizeof(struct kinfo_proc2), &cntp))) {
      vec.push_back(proc_info->p_ppid);
    }
    kvm_close(kd);
    #elif defined(__OpenBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, sizeof(struct kinfo_proc), &cntp))) {
      vec.push_back(proc_info->p_ppid);
    }
    kvm_close(kd);
    if (vec.empty() && proc_id == 0)
      vec.push_back(0);
    #endif
    #if defined(__sun)
    pstatus_t status;
    char buffer[BUFSIZ];
    sprintf(buffer, "/proc/%d/status", proc_id);
    int fd = open(buffer, O_RDONLY);
    if (fd != -1) {
      if (read(fd, &status, sizeof(pstatus_t)) > 0) {
        vec.push_back(status.pr_ppid);
      }
      close(fd);
    }
    kvm_t *kd = nullptr;
    struct proc *proc_info = nullptr;
    if (!vec.empty()) { 
      goto finish;
    }
    kd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc(kd, proc_id))) {
      vec.push_back(proc_info->p_ppid);
    }
    kvm_close(kd);
    finish:
    #endif
    return vec;
  }

  std::vector<NGS_PROCID> proc_id_from_parent_proc_id(NGS_PROCID parent_proc_id) {
    std::vector<NGS_PROCID> vec;
    if (parent_proc_id < 0) return vec;
    #if defined(_WIN32)
    HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!hp) return vec;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hp, &pe)) {
      do {
        message_pump();
        if (pe.th32ParentProcessID == parent_proc_id) {
          vec.push_back(pe.th32ProcessID);
        }
      } while (Process32Next(hp, &pe));
    }
    CloseHandle(hp);
    #elif (defined(__APPLE__) && defined(__MACH__))
    std::vector<NGS_PROCID> proc_info;
    proc_info.resize(proc_listpids(PROC_PPID_ONLY, (uint32_t)parent_proc_id, nullptr, 0));
    int cntp = proc_listpids(PROC_PPID_ONLY, (uint32_t)parent_proc_id, &proc_info[0], sizeof(NGS_PROCID) * proc_info.size());
    for (int i = cntp - 1; i >= 0; i--) {
      if (proc_info[i] <= 0) continue;
      if (proc_info[i] == 1 && parent_proc_id == 0) {
        vec.push_back(0);
      }
      vec.push_back(proc_info[i]);
    }
    #elif (defined(__linux__) || defined(__ANDROID__) || defined(__sun))
    std::vector<NGS_PROCID> proc_id = proc_id_enum();
    for (std::size_t i = 0; i < proc_id.size(); i++) {
      std::vector<NGS_PROCID> ppid = parent_proc_id_from_proc_id(proc_id[i]);
      if (!ppid.empty() && ppid[0] == parent_proc_id) {
        vec.push_back(proc_id[i]);
      }
    }
    #elif defined(__FreeBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PROC, 0, &cntp))) {
      for (int i = 0; i < cntp; i++) {
        if (proc_info[i].ki_ppid == parent_proc_id) {
          vec.push_back(proc_info[i].ki_pid);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__DragonFly__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_ALL, 0, &cntp))) {
      for (int i = 0; i < cntp; i++) {
        if (proc_info[i].kp_pid == 1 && proc_info[i].kp_ppid == 0 && parent_proc_id == 0) {
          vec.push_back(0);
        }
        if (proc_info[i].kp_pid >= 0 && proc_info[i].kp_ppid >= 0 && proc_info[i].kp_ppid == parent_proc_id) {
          vec.push_back(proc_info[i].kp_pid);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__NetBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc2 *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc2), &cntp))) {
      for (int i = cntp - 1; i >= 0; i--) {
        if (proc_info[i].p_ppid == parent_proc_id) {
          vec.push_back(proc_info[i].p_pid);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__OpenBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc), &cntp))) {
      for (int i = cntp - 1; i >= 0; i--) {
        if (proc_info[i].p_pid == 1 && proc_info[i].p_ppid == 0 && parent_proc_id == 0) {
          vec.push_back(0);
        }
        if (proc_info[i].p_ppid == parent_proc_id) {
          vec.push_back(proc_info[i].p_pid);
        }
      }
    }
    kvm_close(kd);
    #endif
    #if defined(__sun)
    struct pid cur_pid;
    kvm_t *kd = nullptr;
    struct proc *proc_info = nullptr;
    if (!vec.empty()) { 
      goto finish;
    }
    kd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    while ((proc_info = kvm_nextproc(kd))) {
      if (proc_info->p_ppid == parent_proc_id) {
        if (kvm_kread(kd, (std::uintptr_t)proc_info->p_pidp, &cur_pid, sizeof(cur_pid)) != -1) {
          vec.insert(vec.begin(), cur_pid.pid_id);
        }
      }
    }
    kvm_close(kd);
    finish:
    #endif
    std::sort(vec.begin(), vec.end());
    auto itr = std::unique(vec.begin(), vec.end());
    vec.erase(itr, vec.end());
    return vec;
  }

  std::vector<NGS_PROCID> proc_id_from_exe(std::string exe) {
    std::vector<NGS_PROCID> vec;
    if (exe.empty()) return vec;
    auto fnamecmp = [](std::string fname1, std::string fname2) {
      #if defined(_WIN32)
      std::transform(fname1.begin(), fname1.end(), fname1.begin(), ::toupper);
      std::transform(fname2.begin(), fname2.end(), fname2.begin(), ::toupper);
      std::size_t fp = fname2.find_last_of("\\/");
      bool abspath = (!fname1.empty() && fname1.length() >= 3 && fname1[1] == ':' &&
        (fname1[2] == '\\' || fname1[2] == '/'));
      #else
      std::size_t fp = fname2.find_last_of("/");
      bool abspath = (!fname1.empty() && fname1.length() >= 1 && fname1[0] == '/');
      #endif
      if (fname1.empty() || fname2.empty() || fp == std::string::npos) return false;
      #if defined(_WIN32)
      if (abspath && fname1.length() == 3) return (fname1 == fname2.substr(0, fp + 1));
      #else
      if (abspath && fname1.length() == 1) return (fname1 == fname2.substr(0, fp + 1));
      #endif
      if (abspath) return (fname1 == fname2 || fname1 == fname2.substr(0, fp));
      return (fname1 == fname2.substr(fp + 1));
    };
    std::vector<NGS_PROCID> proc_id = proc_id_enum();
    for (std::size_t i = 0; i < proc_id.size(); i++) {
      if (fnamecmp(exe, exe_from_proc_id(proc_id[i]))) {
        vec.push_back(proc_id[i]);
      }
    }
    return vec;
  }

  std::vector<NGS_PROCID> proc_id_from_cwd(std::string cwd) {
    std::vector<NGS_PROCID> vec;
    if (cwd.empty()) return vec;
    auto fnamecmp = [](std::string fname1, std::string fname2) {
      #if defined(_WIN32)
      std::transform(fname1.begin(), fname1.end(), fname1.begin(), ::toupper);
      std::transform(fname2.begin(), fname2.end(), fname2.begin(), ::toupper);
      #endif
      if (fname1.empty() || fname2.empty()) return false;
      return (fname1 == fname2);
    };
    std::vector<NGS_PROCID> proc_id = proc_id_enum();
    for (std::size_t i = 0; i < proc_id.size(); i++) {
      if (fnamecmp(cwd, cwd_from_proc_id(proc_id[i]))) {
        vec.push_back(proc_id[i]);
      }
    }
    return vec;
  }

  std::string exe_from_proc_id(NGS_PROCID proc_id) {
    std::string path;
    if (proc_id < 0) return path;
    #if defined(_WIN32)
    if (proc_id == GetCurrentProcessId()) {
      wchar_t buffer[MAX_PATH];
      if (GetModuleFileNameW(nullptr, buffer, sizeof(buffer))) {
        wchar_t exe[MAX_PATH];
        if (_wfullpath(exe, buffer, MAX_PATH)) {
          path = narrow(exe);
        }
      }
    } else {
      HANDLE proc = open_process_with_debug_privilege(proc_id);
      if (proc == nullptr) return path;
      wchar_t buffer[MAX_PATH];
      DWORD size = sizeof(buffer);
      if (QueryFullProcessImageNameW(proc, 0, buffer, &size)) {
        wchar_t exe[MAX_PATH];
        if (_wfullpath(exe, buffer, MAX_PATH)) {
          path = narrow(exe);
        }
      }
      CloseHandle(proc);
    }
    #elif (defined(__APPLE__) && defined(__MACH__))
    char exe[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(proc_id, exe, sizeof(exe)) > 0) {
      char buffer[PATH_MAX];
      if (realpath(exe, buffer)) {
        path = buffer;
      }
    }
    #elif (defined(__linux__) || defined(__ANDROID__))
    char exe[PATH_MAX];
    if (realpath(("/proc/" + std::to_string(proc_id) + "/exe").c_str(), exe)) {
      path = exe;
    }
    #elif (defined(__FreeBSD__) || defined(__DragonFly__))
    int mib[4];
    std::size_t len = 0;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = proc_id;
    if (!sysctl(mib, 4, nullptr, &len, nullptr, 0)) {
      std::vector<char> vecbuff;
      vecbuff.resize(len);
      char *exe = &vecbuff[0];
      if (!sysctl(mib, 4, exe, &len, nullptr, 0)) {
        char buffer[PATH_MAX];
        if (realpath(exe, buffer)) {
          path = buffer;
        }
      }
    }
    #elif defined(__NetBSD__)
    int mib[4];
    std::size_t len = 0;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_ARGS;
    mib[2] = proc_id;
    mib[3] = KERN_PROC_PATHNAME;
    if (!sysctl(mib, 4, nullptr, &len, nullptr, 0)) {
      std::vector<char> vecbuff;
      vecbuff.resize(len);
      char *exe = &vecbuff[0];
      if (!sysctl(mib, 4, exe, &len, nullptr, 0)) {
        char buffer[PATH_MAX];
        if (realpath(exe, buffer)) {
          path = buffer;
        }
      }
    }
    #elif defined(__OpenBSD__)
    auto is_exe = [](NGS_PROCID proc_id, std::string exe) {
      int cntp = 0;
      std::string res;
      kvm_t *kd = nullptr;
      kinfo_file *kif = nullptr;
      bool error = false;
      kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
      if (!kd) return res;
      if ((kif = kvm_getfiles(kd, KERN_FILE_BYPID, proc_id, sizeof(struct kinfo_file), &cntp))) {
        for (int i = 0; i < cntp && kif[i].fd_fd < 0; i++) {
          if (kif[i].fd_fd == KERN_FILE_TEXT) {
            struct stat st;
            fallback:
            char buffer[PATH_MAX];
            if (!stat(exe.c_str(), &st) && (st.st_mode & S_IXUSR) &&
              (st.st_mode & S_IFREG) && realpath(exe.c_str(), buffer) &&
              st.st_dev == (dev_t)kif[i].va_fsid && st.st_ino == (ino_t)kif[i].va_fileid) {
              res = buffer;
            }
            if (res.empty() && !error) {
              error = true;
              std::size_t last_slash_pos = exe.find_last_of("/");
              if (last_slash_pos != std::string::npos) {
                exe = exe.substr(0, last_slash_pos + 1) + kif[i].p_comm;
                goto fallback;
              }
            }
            break;
          }
        }
      }
      kvm_close(kd);
      return res;
    };
    bool error = false, retried = false;
    std::vector<std::string> buffer = cmdline_from_proc_id(proc_id);
    if (!buffer.empty()) {
      std::string argv0;
      if (!buffer[0].empty()) {
        fallback:
        std::size_t slash_pos = buffer[0].find('/');
        std::size_t colon_pos = buffer[0].find(':');
        if (slash_pos == 0) {
          argv0 = buffer[0];
          path = is_exe(proc_id, argv0);
        } else if (slash_pos == std::string::npos || slash_pos > colon_pos) { 
          std::string penv = envvar_value_from_proc_id(proc_id, "PATH");
          if (!penv.empty()) {
            retry:
            std::string tmp;
            std::stringstream sstr(penv);
            while (std::getline(sstr, tmp, ':')) {
              argv0 = tmp + "/" + buffer[0];
              path = is_exe(proc_id, argv0);
              if (!path.empty()) break;
              if (slash_pos > colon_pos) {
                argv0 = tmp + "/" + buffer[0].substr(0, colon_pos);
                path = is_exe(proc_id, argv0);
                if (!path.empty()) break;
              }
            }
          }
          if (path.empty() && !retried) {
            retried = true;
            penv = "/usr/bin:/bin:/usr/sbin:/sbin:/usr/X11R6/bin:/usr/local/bin:/usr/local/sbin";
            std::string home = envvar_value_from_proc_id(proc_id, "HOME");
            if (!home.empty()) {
              penv = home + "/bin:" + penv;
            }
            goto retry;
          }
        }
        if (path.empty() && slash_pos > 0) {
          std::string pwd = envvar_value_from_proc_id(proc_id, "PWD");
          if (!pwd.empty()) {
            argv0 = pwd + "/" + buffer[0];
            path = is_exe(proc_id, argv0);
          }
          if (path.empty()) {
            std::string cwd = cwd_from_proc_id(proc_id);
            if (!cwd.empty()) {
              argv0 = cwd + "/" + buffer[0];
              path = is_exe(proc_id, argv0);
            }
          }
        }
      }
      if (path.empty() && !error) {
        error = true;
        buffer.clear();
        std::string underscore = envvar_value_from_proc_id(proc_id, "_");
        if (!underscore.empty()) {
          buffer.push_back(underscore);
          goto fallback;
        }
      }
    }
    if (!path.empty()) {
      errno = 0;
    }
    #elif defined(__sun)
    int err = 0;
    char exe[PATH_MAX];
    struct ps_prochandle *P = Pgrab(proc_id, PGRAB_RDONLY, &err);
    if (P) {
      if (!err && !errno) {
        char buffer[PATH_MAX];
        if (Pexecname(P, buffer, sizeof(buffer))) {
          if (realpath(buffer, exe)) {
            path = exe;
          }
        }
      }
      Pfree(P);
    }
    if (!path.empty()) {
      goto finish;
    }
    if (realpath(("/proc/" + std::to_string(proc_id) + "/path/a.out").c_str(), exe)) {
      path = exe;
    }
    finish:
    #endif
    return path;
  }

  std::string cwd_from_proc_id(NGS_PROCID proc_id) {
    std::string path;
    if (proc_id < 0) return path;
    #if defined(_WIN32)
    HANDLE proc = open_process_with_debug_privilege(proc_id);
    if (proc == nullptr) return path;
    std::vector<wchar_t> buffer = cmd_env_cwd_from_proc(proc, MEMCWD);
    if (!buffer.empty()) {
      wchar_t cwd[MAX_PATH];
      if (_wfullpath(cwd, &buffer[0], MAX_PATH)) {
        path = narrow(cwd);
        if (!path.empty() && std::count(path.begin(), path.end(), '\\') > 1 && path.back() == '\\') {
          path = path.substr(0, path.length() - 1);
        }
      }
    }
    CloseHandle(proc);
    #elif (defined(__APPLE__) && defined(__MACH__))
    proc_vnodepathinfo vpi;
    if (proc_pidinfo(proc_id, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi)) > 0) {
      char buffer[PATH_MAX];
      if (realpath(vpi.pvi_cdir.vip_path, buffer)) {
        path = buffer;
      }
    }
    #elif (defined(__linux__) || defined(__ANDROID__))
    char cwd[PATH_MAX];
    if (realpath(("/proc/" + std::to_string(proc_id) + "/cwd").c_str(), cwd)) {
      path = cwd;
    }
    #elif defined(__FreeBSD__)
    int mib[4];
    struct kinfo_file kif;
    std::size_t len = sizeof(kif);
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_CWD;
    mib[3] = proc_id;
    if (!sysctl(mib, 4, nullptr, &len, nullptr, 0)) {
      memset(&kif, 0, len);
      if (!sysctl(mib, 4, &kif, &len, nullptr, 0)) {
        char buffer[PATH_MAX];
        if (realpath(kif.kf_path, buffer)) {
           path = buffer;
        }
      }
    }
    #elif defined(__DragonFly__)
    int mib[4];
    char cwd[PATH_MAX];
    std::size_t len = sizeof(cwd);
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_CWD;
    mib[3] = proc_id;
    if (!sysctl(mib, 4, cwd, &len, nullptr, 0)) {
      char buffer[PATH_MAX];
      if (realpath(cwd, buffer)) {
        path = buffer;
      }
    }
    #elif defined(__NetBSD__)
    int mib[4];
    std::size_t len = 0;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_ARGS;
    mib[2] = proc_id;
    mib[3] = KERN_PROC_CWD;
    if (!sysctl(mib, 4, nullptr, &len, nullptr, 0)) {
      std::vector<char> vecbuff;
      vecbuff.resize(len);
      char *cwd = &vecbuff[0];
      if (!sysctl(mib, 4, cwd, &len, nullptr, 0)) {
        char buffer[PATH_MAX];
        if (realpath(cwd, buffer)) {
          path = buffer;
        }
      }
    }
    #elif defined(__OpenBSD__)
    int mib[3];
    std::size_t len = 0;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_CWD;
    mib[2] = proc_id;
    if (!sysctl(mib, 3, nullptr, &len, nullptr, 0)) {
      std::vector<char> vecbuff;
      vecbuff.resize(len);
      char *cwd = &vecbuff[0];
      if (!sysctl(mib, 3, cwd, &len, nullptr, 0)) {
        char buffer[PATH_MAX];
        if (realpath(cwd, buffer)) {
          path = buffer;
        }
      }
    }
    #elif defined(__sun)
    char cwd[PATH_MAX];
    if (realpath(("/proc/" + std::to_string(proc_id) + "/path/cwd").c_str(), cwd)) {
      path = cwd;
    }
    #endif
    return path;
  }

  std::string comm_from_proc_id(NGS_PROCID proc_id) {
    std::string exe = exe_from_proc_id(proc_id);
    if (exe.empty()) return "";
    #if !defined(_WIN32)
    std::size_t pos = exe.find_last_of("/");
    #else
    std::size_t pos = exe.find_last_of("\\/");
    #endif
    if (pos != std::string::npos) {
      return exe.substr(pos + 1);
    }
    return exe;
  }

  std::vector<std::string> cmdline_from_proc_id(NGS_PROCID proc_id) {
    std::vector<std::string> vec;
    if (proc_id < 0) return vec;
    #if defined(_WIN32)
    HANDLE proc = open_process_with_debug_privilege(proc_id);
    if (proc == nullptr) return vec;
    int cmdsize = 0;
    std::vector<wchar_t> buffer = cmd_env_cwd_from_proc(proc, MEMCMD);
    if (!buffer.empty()) {
      wchar_t **cmd = CommandLineToArgvW(&buffer[0], &cmdsize);
      if (cmd) {
        for (int i = 0; i < cmdsize; i++) {
          message_pump();
          vec.push_back(narrow(cmd[i]));
        }
        LocalFree(cmd);
      }
    }
    CloseHandle(proc);
    #elif (defined(__APPLE__) && defined(__MACH__))
    vec = cmd_env_from_proc_id(proc_id, MEMCMD);
    #elif (defined(__linux__) || defined(__ANDROID__) || defined(__sun))
    FILE *file = fopen(("/proc/" + std::to_string(proc_id) + "/cmdline").c_str(), "rb");
    if (file) {
      char *cmd = nullptr;
      std::size_t size = 0;
      while (getdelim(&cmd, &size, 0, file) != -1) {
        vec.push_back(cmd);
      }
      while (!vec.empty() && vec.back().empty())
        vec.pop_back();
      if (cmd) free(cmd);
      fclose(file);
    }
    #elif (defined(__FreeBSD__) || defined(__DragonFly__))
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, &cntp))) {
      char **cmd = kvm_getargv(kd, proc_info, 0);
      if (cmd) {
        for (int i = 0; cmd[i]; i++) {
          vec.push_back(cmd[i]);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__NetBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc2 *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc2(kd, KERN_PROC_PID, proc_id, sizeof(struct kinfo_proc2), &cntp))) {
      char **cmd = kvm_getargv2(kd, proc_info, 0);
      if (cmd) {
        for (int i = 0; cmd[i]; i++) {
          vec.push_back(cmd[i]);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__OpenBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, sizeof(struct kinfo_proc), &cntp))) {
      char **cmd = kvm_getargv(kd, proc_info, 0);
      if (cmd) {
        for (int i = 0; cmd[i]; i++) {
          vec.push_back(cmd[i]);
        }
      }
    }
    kvm_close(kd);
    #endif
    #if defined(__sun)
    if (vec.empty()) {
      vec = cmd_env_from_proc_id(proc_id, MEMCMD);
    }
    kvm_t *kd = nullptr;
    char **cmd = nullptr;
    struct proc *proc_info = nullptr;
    struct user *proc_user = nullptr;
    if (!vec.empty()) { 
      goto finish;
    }
    kd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc(kd, proc_id))) {
      if ((proc_user = kvm_getu(kd, proc_info))) {
        if (!kvm_getcmd(kd, proc_info, proc_user, &cmd, nullptr)) {
          for (int i = 0; cmd[i]; i++) {
            vec.push_back(cmd[i]);
          }
          free(cmd);
        }
      }
    }
    kvm_close(kd);
    finish:
    #endif
    return vec;
  }

  std::vector<std::string> environ_from_proc_id(NGS_PROCID proc_id) {
    std::vector<std::string> vec;
    if (proc_id < 0) return vec;
    #if defined(_WIN32)
    HANDLE proc = open_process_with_debug_privilege(proc_id);
    if (proc == nullptr) return vec;
    std::vector<wchar_t> buffer = cmd_env_cwd_from_proc(proc, MEMENV);
    int i = 0;
    if (!buffer.empty()) {
      while (buffer[i] != L'\0') {
        message_pump();
        vec.push_back(narrow(&buffer[i]));
        i += (int)(wcslen(&buffer[0] + i) + 1);
      }
    }
    CloseHandle(proc);
    #elif (defined(__APPLE__) && defined(__MACH__))
    vec = cmd_env_from_proc_id(proc_id, MEMENV);
    #elif (defined(__linux__) || defined(__ANDROID__) || defined(__sun))
    FILE *file = fopen(("/proc/" + std::to_string(proc_id) + "/environ").c_str(), "rb");
    if (file) {
      char *env = nullptr;
      std::size_t size = 0;
      while (getdelim(&env, &size, 0, file) != -1) {
        vec.push_back(env);
      }
      if (env) free(env);
      fclose(file);
    }
    #elif (defined(__FreeBSD__) || defined(__DragonFly__))
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    const char *nlistf = "/dev/null";
    const char *memf   = "/dev/null";
    kd = kvm_openfiles(nlistf, memf, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, &cntp))) {
      char **env = kvm_getenvv(kd, proc_info, 0);
      if (env) {
        for (int i = 0; env[i]; i++) {
          vec.push_back(env[i]);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__NetBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc2 *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc2(kd, KERN_PROC_PID, proc_id, sizeof(struct kinfo_proc2), &cntp))) {
      char **env = kvm_getenvv2(kd, proc_info, 0);
      if (env) {
        for (int i = 0; env[i]; i++) {
          vec.push_back(env[i]);
        }
      }
    }
    kvm_close(kd);
    #elif defined(__OpenBSD__)
    int cntp = 0;
    kvm_t *kd = nullptr;
    kinfo_proc *proc_info = nullptr;
    kd = kvm_openfiles(nullptr, nullptr, nullptr, KVM_NO_FILES, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getprocs(kd, KERN_PROC_PID, proc_id, sizeof(struct kinfo_proc), &cntp))) {
      char **env = kvm_getenvv(kd, proc_info, 0);
      if (env) {
        for (int i = 0; env[i]; i++) {
          vec.push_back(env[i]);
        }
      }
    }
    kvm_close(kd);
    #endif
    #if defined(__sun)
    if (vec.empty()) {
      vec = cmd_env_from_proc_id(proc_id, MEMENV);
    }
    kvm_t *kd = nullptr;
    char **env = nullptr;
    struct proc *proc_info = nullptr;
    struct user *proc_user = nullptr;
    if (!vec.empty()) { 
      goto finish;
    }
    kd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, nullptr);
    if (!kd) return vec;
    if ((proc_info = kvm_getproc(kd, proc_id))) {
      if ((proc_user = kvm_getu(kd, proc_info))) {
        if (!kvm_getcmd(kd, proc_info, proc_user, nullptr, &env)) {
          for (int i = 0; env[i]; i++) {
            vec.push_back(env[i]);
          }
          free(env);
        }
      }
    }
    kvm_close(kd);
    finish:
    #endif
    struct is_invalid {
      bool operator()(const std::string &s) {
        return (s.find('=') == std::string::npos);
      }
    };
    vec.erase(std::remove_if(vec.begin(), vec.end(), is_invalid()), vec.end());
    return vec;
  }

  std::string envvar_value_from_proc_id(NGS_PROCID proc_id, std::string name) {
    std::string value;
    if (proc_id < 0 || name.empty()) return value;
    std::vector<std::string> vec = environ_from_proc_id(proc_id);
    if (!vec.empty()) {
      for (std::size_t i = 0; i < vec.size(); i++) {
        message_pump();
        std::vector<std::string> equalssplit = string_split_by_first_equals_sign(vec[i]);
        if (equalssplit.size() == 2) {
          #if defined(_WIN32)
          std::transform(equalssplit[0].begin(), equalssplit[0].end(), equalssplit[0].begin(), ::toupper);
          std::transform(name.begin(), name.end(), name.begin(), ::toupper);
          #endif
          if (equalssplit[0] == name) {
            value = equalssplit[1];
            break;
          }
        }
      }
    }
    return value;
  }

  bool envvar_exists_from_proc_id(NGS_PROCID proc_id, std::string name) {
    bool exists = false;
    if (proc_id < 0 || name.empty()) return exists;
    std::vector<std::string> vec = environ_from_proc_id(proc_id);
    if (!vec.empty()) {
      for (std::size_t i = 0; i < vec.size(); i++) {
        message_pump();
        std::vector<std::string> equalssplit = string_split_by_first_equals_sign(vec[i]);
        if (!equalssplit.empty()) {
          #if defined(_WIN32)
          std::transform(equalssplit[0].begin(), equalssplit[0].end(), equalssplit[0].begin(), ::toupper);
          std::transform(name.begin(), name.end(), name.begin(), ::toupper);
          #endif
          if (equalssplit[0] == name) {
            exists = true;
            break;
          }
        }
      }
    }
    return exists;
  }

  namespace {

    std::unordered_map<NGS_PROCID, std::intptr_t> stdipt_map;
    std::unordered_map<NGS_PROCID, std::string> stdopt_map;
    std::unordered_map<NGS_PROCID, bool> complete_map;
    std::unordered_map<int, NGS_PROCID> child_proc_id;
    std::unordered_map<int, bool> proc_did_execute;
    std::mutex complete_mutex;
    std::mutex stdopt_mutex;
    long long optlmt = 0;
    int index = -1;

    #if !defined(_WIN32)
    NGS_PROCID process_execute_helper(const char *command, int *infp, int *outfp) {
      int p_stdin[2];
      int p_stdout[2];
      NGS_PROCID pid = -1;
      if (pipe(p_stdin) == -1)
        return -1;
      if (pipe(p_stdout) == -1) {
        close(p_stdin[0]);
        close(p_stdin[1]);
        return -1;
      }
      pid = fork();
      if (pid < 0) {
        close(p_stdin[0]);
        close(p_stdin[1]);
        close(p_stdout[0]);
        close(p_stdout[1]);
        return pid;
      } else if (pid == 0) {
        close(p_stdin[1]);
        dup2(p_stdin[0], 0);
        close(p_stdout[0]);
        dup2(p_stdout[1], 1);
        #if defined(NULLIFY_STDERR)
        dup2(::open("/dev/null", O_RDONLY), 2);
        #else
        dup2(p_stdout[1], 2);
        #endif
        for (int i = 3; i < 4096; i++)
          close(i);
        setsid();
        #if !defined(__ANDROID__)
        execl("/bin/sh", "sh", "-c", command, nullptr);
        #else
        execl("/system/bin/sh", "sh", "-c", command, nullptr);
        #endif
        _exit(-1);
      }
      close(p_stdin[0]);
      close(p_stdout[1]);
      if (infp == nullptr) {
        close(p_stdin[1]);
      } else {
        *infp = p_stdin[1];
      }
      if (outfp == nullptr) {
        close(p_stdout[0]);
      } else {
        *outfp = p_stdout[0];
      }
      return pid;
    }
    #endif

    void output_thread(std::intptr_t file, NGS_PROCID proc_index) {
      #if !defined(_WIN32)
      ssize_t nRead = 0; char buffer[BUFSIZ];
      while ((nRead = read((int)file, buffer, BUFSIZ)) > 0) {
        buffer[nRead] = '\0';
      #else
      DWORD nRead = 0; char buffer[BUFSIZ];
      while (ReadFile((HANDLE)(void *)file, buffer, BUFSIZ, &nRead, nullptr) && nRead) {
        message_pump();
        buffer[nRead] = '\0';
      #endif
        std::lock_guard<std::mutex> guard(stdopt_mutex);
        stdopt_map[proc_index].append(buffer, nRead);
        long long limit = stdopt_map[proc_index].length() -
        ((optlmt) ? optlmt : stdopt_map[proc_index].length());
        limit = ((limit < 0) ? 0 : limit);
        stdopt_map[proc_index] = stdopt_map[proc_index].substr(limit);
      }
    }

    #if !defined(_WIN32)
    NGS_PROCID proc_id_from_fork_proc_id(NGS_PROCID proc_id) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      std::vector<NGS_PROCID> ppid = proc_id_from_parent_proc_id(proc_id);
      if (!ppid.empty()) proc_id = ppid[0];
      return proc_id;
    }
    #endif

    NGS_PROCID spawn_child_proc_id_helper(std::string command) {
      index++;
      #if !defined(_WIN32)
      int infd = 0, outfd = 0;
      NGS_PROCID proc_id = 0, fork_proc_id = 0, wait_proc_id = 0;
      fork_proc_id = process_execute_helper(command.c_str(), &infd, &outfd);
      proc_id = fork_proc_id; wait_proc_id = proc_id;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      if (fork_proc_id != -1) {
        while ((proc_id = proc_id_from_fork_proc_id(proc_id)) == wait_proc_id) {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          int status = 0; wait_proc_id = waitpid(fork_proc_id, &status, WNOHANG);
          std::string exe = exe_from_proc_id(fork_proc_id);
          if (exe.empty()) {
            break;
          }
          char shlbuf[PATH_MAX];
          #if !defined(__ANDROID__)
          const char *shl = "/bin/sh";
          #else
          const char *shl = "/system/bin/sh";
          #endif
          if (realpath(shl, shlbuf)) {
            if (strcmp(exe.c_str(), shlbuf) == 0) {
              if (wait_proc_id > 0)
                proc_id = wait_proc_id;
            } else {
              break;
            }
          } else {
            break;
          }
        }
      }
      child_proc_id[index] = proc_id; std::this_thread::sleep_for(std::chrono::milliseconds(5));
      proc_did_execute[index] = true; NGS_PROCID proc_index = proc_id;
      stdipt_map[proc_index] = (std::intptr_t)infd;
      std::thread opt_thread(output_thread, (std::intptr_t)outfd, proc_index);
      opt_thread.join();
      #else
      std::wstring wstr_command = widen(command); bool proceed = true;
      wchar_t *cwstr_command = new wchar_t[wstr_command.length() + 1]();
      wcsncpy_s(cwstr_command, wstr_command.length() + 1, wstr_command.c_str(), wstr_command.length() + 1);
      HANDLE stdin_read = nullptr; HANDLE stdin_write = nullptr;
      HANDLE stdout_read = nullptr; HANDLE stdout_write = nullptr;
      SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, true };
      proceed = CreatePipe(&stdin_read, &stdin_write, &sa, 0);
      if (proceed == false) return 0;
      SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
      proceed = CreatePipe(&stdout_read, &stdout_write, &sa, 0);
      if (proceed == false) return 0;
      STARTUPINFOW si;
      ZeroMemory(&si, sizeof(si));
      si.cb = sizeof(STARTUPINFOW);
      si.dwFlags = STARTF_USESTDHANDLES;
      #if defined(NULLIFY_STDERR)
      si.hStdError = nullptr;
      #else
      si.hStdError = stdout_write;
      #endif
      si.hStdOutput = stdout_write;
      si.hStdInput = stdin_read;
      PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi)); NGS_PROCID proc_index = 0;
      BOOL success = CreateProcessW(nullptr, cwstr_command, nullptr, nullptr, true, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
      delete[] cwstr_command;
      if (success) {
        CloseHandle(stdout_write);
        CloseHandle(stdin_read);
        NGS_PROCID proc_id = pi.dwProcessId; child_proc_id[index] = proc_id; proc_index = proc_id;
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); proc_did_execute[index] = true;
        stdipt_map[proc_index] = (std::intptr_t)(void *)stdin_write;
        HANDLE wait_handles[] = { pi.hProcess, stdout_read };
        std::thread opt_thread(output_thread, (std::intptr_t)(void *)stdout_read, proc_index);
        while (MsgWaitForMultipleObjects(2, wait_handles, false, 5, QS_ALLEVENTS) != WAIT_OBJECT_0) {
          message_pump();
        }
        opt_thread.join();
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(stdout_read);
        CloseHandle(stdin_write);
      } else {
        proc_did_execute[index] = true;
        child_proc_id[index] = 0;
      }
      #endif
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::lock_guard<std::mutex> guard(complete_mutex);
      complete_map[proc_index] = true;
      return proc_index;
    }

  } // anonymous namespace

  NGS_PROCID spawn_child_proc_id(std::string command, bool wait) {
    #if (defined(USE_SDL2_POLLEVENT) || defined(USE_SDL3_POLLEVENT))
    if (wait) {
      int prevIndex = index;
      std::thread proc_thread(spawn_child_proc_id_helper, command);
      while (prevIndex == index) {
        message_pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      while (proc_did_execute.find(index) == proc_did_execute.end() || !proc_did_execute[index]) {
        message_pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      NGS_PROCID proc_index = child_proc_id[index];
      SDL_Event event;
      while (true) {
        std::lock_guard<std::mutex> guard(complete_mutex);
        if (complete_map[proc_index]) break;
        while (SDL_PollEvent(&event) > 0);
      }
      proc_thread.join();
      return proc_index;
    }
    #else
    if (wait) return spawn_child_proc_id_helper(command);
    #endif
    int prevIndex = index;
    std::thread proc_thread(spawn_child_proc_id_helper, command);
    while (prevIndex == index) {
      message_pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    while (proc_did_execute.find(index) == proc_did_execute.end() || !proc_did_execute[index]) {
      message_pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    NGS_PROCID proc_index = child_proc_id[index];
    std::lock_guard<std::mutex> guard(complete_mutex);
    complete_map[proc_index] = false;
    proc_thread.detach();
    return proc_index;
  }

  void stdout_set_buffer_limit(long long limit) {
    optlmt = limit;
  }

  std::string read_from_stdout_for_child_proc_id(NGS_PROCID proc_id) {
    std::lock_guard<std::mutex> guard(stdopt_mutex);
    if (stdopt_map.find(proc_id) == stdopt_map.end()) return "";
    return stdopt_map.find(proc_id)->second.c_str();
  }

  long long write_to_stdin_for_child_proc_id(NGS_PROCID proc_id, std::string input) {
    if (stdipt_map.find(proc_id) == stdipt_map.end()) return -1;
    std::string s = input;
    std::vector<char> v(s.length());
    std::copy(s.c_str(), s.c_str() + s.length(), v.begin());
    #if !defined(_WIN32)
    ssize_t nwritten = -1;
    lseek((int)stdipt_map[proc_id], 0, SEEK_END);
    nwritten = write((int)stdipt_map[proc_id], &v[0], v.size());
    return nwritten;
    #else
    DWORD dwwritten = -1;
    SetFilePointer((HANDLE)(void *)stdipt_map[proc_id], 0, nullptr, FILE_END);
    WriteFile((HANDLE)(void *)stdipt_map[proc_id], &v[0], (DWORD)v.size(), &dwwritten, nullptr);
    return (((long long)(DWORD)-1 != (long long)dwwritten) ? dwwritten : -1);
    #endif
  }

  bool child_proc_id_is_complete(NGS_PROCID proc_id) {
    std::lock_guard<std::mutex> guard(complete_mutex);
    if (complete_map.find(proc_id) == complete_map.end()) return false;
    return complete_map.find(proc_id)->second;
  }

  std::string read_from_stdin_for_self() {
    std::string standard_input;
    #if defined(_WIN32)
    if (_isatty(_fileno(stdin))) {
      return standard_input;
    }
    std::vector<char> buff;
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
      return standard_input;
    }
    if (GetFileType(handle) == FILE_TYPE_PIPE) {
      DWORD mode = 0;
      if (GetConsoleMode(handle, &mode)) {
        DWORD bytes_avail = 0;
        if (PeekNamedPipe(handle, nullptr, 0, nullptr, &bytes_avail, nullptr)) {
          DWORD bytes_read = 0;
          buff.resize(bytes_avail);
          if (PeekNamedPipe(handle, &buff[0], bytes_avail, &bytes_read, nullptr, nullptr)) {
            standard_input = buff.data();
          }
        }
      } else {
        DWORD nRead = BUFSIZ;
        buff.resize(nRead);
        while (ReadFile(handle, &buff[0], nRead, &nRead, nullptr) && nRead) {
          message_pump();
          standard_input.append(buff.data(), nRead);
        }
      }
    } else {
      DWORD mode = 0;
      if (GetConsoleMode(handle, &mode)) {
        struct stat st;
        if (!fstat(_fileno(stdin), &st)) {
          std::vector<wchar_t> buff;
          buff.resize(st.st_size + 1);
          if (fgetws(&buff[0], st.st_size + 1, stdin)) {
            std::wstring wstr(buff.begin(), buff.end());
            standard_input = narrow(wstr);
          }
        }
      } else {
        DWORD nRead = BUFSIZ;
        buff.resize(nRead);
        while (ReadFile(handle, &buff[0], nRead, &nRead, nullptr) && nRead) {
          message_pump();
          standard_input.append(buff.data(), nRead);
        }
      }
    }
    #else
    if (isatty(fileno(stdin))) {
      return standard_input;
    }
    std::vector<char> buff;
    ssize_t nread = BUFSIZ;
    buff.resize(nread);
    while ((nread = read(fileno(stdin), &buff[0], nread)) > 0) {
      standard_input.append(buff.data(), nread);
    }
    #endif
    return standard_input;
  }

  bool free_stdout_for_child_proc_id(NGS_PROCID proc_id) {
    if (stdopt_map.find(proc_id) == stdopt_map.end()) return false;
    stdopt_map.erase(proc_id);
    return true;
  }

  bool free_stdin_for_child_proc_id(NGS_PROCID proc_id) {
    if (stdipt_map.find(proc_id) == stdipt_map.end()) return false;
    stdipt_map.erase(proc_id);
    return true;
  }

} // namespace ngs::ps
