/**
 * @file command_runner.hpp
 * @brief 提供同步执行外部命令并捕获输出的接口。
 */

#pragma once

#include <cstdint>
#include <string>

namespace platform {

/**
 * @brief 外部命令的执行结果。
 */
struct CommandResult {
  bool launched = false;       /**< CreateProcessW 是否成功。 */
  std::uint32_t error = 0;     /**< 失败时的 GetLastError()。 */
  std::uint32_t exit_code = 0; /**< 子进程退出码。 */
  std::string output;          /**< stdout 与 stderr 的合并输出。 */
};

/**
 * @brief 同步执行命令并等待其结束。
 * @param[in] command_line 完整命令行，例如 L"usbipd.exe list"。
 * @return 执行结果；函数返回时子进程已经结束。
 * @note 该函数会阻塞，调用方应放到后台线程。
 */
CommandResult RunCommand(const std::wstring& command_line);

}  // namespace platform
