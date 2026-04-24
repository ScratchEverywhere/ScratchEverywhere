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

#pragma once
#include <vector>
#include <string>

namespace ngs::ps {

  #if !defined(_WIN32)
  typedef int NGS_PROCID;
  #else
  typedef unsigned long NGS_PROCID;
  #endif

  NGS_PROCID proc_id_from_self();
  std::vector<NGS_PROCID> proc_id_enum();
  bool proc_id_exists(NGS_PROCID proc_id);
  bool proc_id_suspend(NGS_PROCID proc_id);
  bool proc_id_resume(NGS_PROCID proc_id);
  bool proc_id_kill(NGS_PROCID proc_id);
  std::vector<NGS_PROCID> parent_proc_id_from_proc_id(NGS_PROCID proc_id);
  std::vector<NGS_PROCID> proc_id_from_parent_proc_id(NGS_PROCID parent_proc_id);
  std::vector<NGS_PROCID> proc_id_from_exe(std::string exe);
  std::vector<NGS_PROCID> proc_id_from_cwd(std::string cwd);
  std::string exe_from_proc_id(NGS_PROCID proc_id);
  std::string cwd_from_proc_id(NGS_PROCID proc_id);
  std::string comm_from_proc_id(NGS_PROCID proc_id);
  std::vector<std::string> cmdline_from_proc_id(NGS_PROCID proc_id);
  std::vector<std::string> environ_from_proc_id(NGS_PROCID proc_id);
  std::string envvar_value_from_proc_id(NGS_PROCID proc_id, std::string name);
  bool envvar_exists_from_proc_id(NGS_PROCID proc_id, std::string name);
  NGS_PROCID spawn_child_proc_id(std::string command, bool wait);
  void stdout_set_buffer_limit(long long limit);
  std::string read_from_stdout_for_child_proc_id(NGS_PROCID proc_id);
  long long write_to_stdin_for_child_proc_id(NGS_PROCID proc_id, std::string input);
  bool child_proc_id_is_complete(NGS_PROCID proc_id);
  std::string read_from_stdin_for_self();
  bool free_stdout_for_child_proc_id(NGS_PROCID proc_id);
  bool free_stdin_for_child_proc_id(NGS_PROCID proc_id);

} // namespace ngs::ps
