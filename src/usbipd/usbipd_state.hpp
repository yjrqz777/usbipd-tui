/**
 * @file usbipd_state.hpp
 * @brief 定义 usbipd 设备状态模型，以及查询与解析接口。
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace usbipd {

/**
 * @brief usbipd state 中的单个设备信息。
 */
typedef struct UsbDeviceDef {
  std::string bus_id;           /**< 当前连接的 BusId；未连接时为空。 */
  std::string vid_pid;          /**< 从 InstanceId 提取的 VID:PID，例如 09DA:2268。 */
  std::string description;      /**< 设备描述，对应列表的 description 列。 */
  std::string persisted_guid;   /**< 持久化绑定 GUID；没有绑定时为空。 */
  std::string stub_instance_id; /**< 绑定后的 stub 设备实例 ID；没有绑定时为空。 */
  std::string client_ip;        /**< 已 attach 时的客户端 IP；未 attach 时为空。 */
  bool is_forced = false;       /**< 是否通过 bind --force 强制绑定。 */
}UsbDeviceDef;

// extern UsbDeviceDef UsbDevice;
/** @brief 查询一次设备状态并写入日志，保留给调试入口使用。 */
void usbipd_state();


/**
 * @brief 把 `usbipd state` 的 JSON 文本解析成设备列表。
 * @param[in] json_text 命令输出的原始 JSON 文本。
 * @return 解析出的设备列表；JSON 非法或缺少 Devices 字段时返回空列表。
 * @note 本函数不抛异常，失败原因写入日志。
 */
std::vector<UsbDeviceDef> ParseUsbipdState(std::string_view json_text);

/**
 * @brief 执行 `usbipd.exe state` 并返回解析后的设备列表。
 * @return 设备列表；命令启动失败或输出无法解析时返回空列表。
 * @warning 该函数内部同步创建子进程，耗时为百毫秒级，不要放在 UI 线程调用。
 */
std::vector<UsbDeviceDef> QueryState();

}  // namespace usbipd
