/**
 * @file usbipd_state.cpp
 * @brief 实现 usbipd 设备状态的查询与 JSON 解析。
 */

#include "usbipd/usbipd_state.hpp"

#include <cstddef>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "platform/command_runner.hpp"

namespace usbipd {
namespace {

/**
 * @brief 读取可为 null 的字符串字段。
 * @param[in] item JSON 对象。
 * @param[in] key 字段名。
 * @return 字段值；字段缺失、为 null 或类型不符时返回空串。
 */
std::string ReadString(const nlohmann::json& item, const char* key) {
  const auto it = item.find(key);
  if (it == item.end() || !it->is_string()) {
    return {};
  }
  return it->get<std::string>();
}

/**
 * @brief 读取可为 null 的布尔字段。
 * @param[in] item JSON 对象。
 * @param[in] key 字段名。
 * @return 字段值；字段缺失、为 null 或类型不符时返回 false。
 */
bool ReadBool(const nlohmann::json& item, const char* key) {
  const auto it = item.find(key);
  if (it == item.end() || !it->is_boolean()) {
    return false;
  }
  return it->get<bool>();
}

/**
 * @brief 从 InstanceId 中提取 VID:PID。
 * @param[in] instance_id 形如 USB\VID_09DA&PID_2268\5&201B4142&0&4 的实例 ID。
 * @return 形如 09DA:2268 的字符串；无法识别时返回空串。
 */
std::string ExtractVidPid(const std::string& instance_id) {
  constexpr std::size_t kHexDigits = 4;                 /* VID/PID 各占 4 位十六进制 */
  constexpr std::size_t kPrefixLength = 4;              /* "VID_" / "PID_" 的长度 */
  constexpr std::size_t kFieldLength = kPrefixLength + kHexDigits;

  const auto vid_pos = instance_id.find("VID_");
  const auto pid_pos = instance_id.find("PID_");
  if (vid_pos == std::string::npos || pid_pos == std::string::npos) {
    return {};
  }
  if (vid_pos + kFieldLength > instance_id.size() ||
      pid_pos + kFieldLength > instance_id.size()) {
    return {};
  }
  return instance_id.substr(vid_pos + kPrefixLength, kHexDigits) + ":" +
         instance_id.substr(pid_pos + kPrefixLength, kHexDigits);
}

}  // namespace

std::vector<UsbDeviceDef> ParseUsbipdState(std::string_view json_text) {
  /* allow_exceptions=false：解析失败返回 discarded，异常不越过本层边界。 */
  const auto root =
      nlohmann::json::parse(json_text.begin(), json_text.end(), nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    spdlog::error("usbipd state output is not valid json");
    return {};
  }

  const auto devices_it = root.find("Devices");
  if (devices_it == root.end() || !devices_it->is_array()) {
    spdlog::error("usbipd state json has no Devices array");
    return {};
  }

  std::vector<UsbDeviceDef> devices;
  devices.reserve(devices_it->size());
  for (const auto& item : *devices_it) {
    if (!item.is_object()) {
      continue;
    }
    UsbDeviceDef device;
    device.bus_id = ReadString(item, "BusId");
    device.vid_pid = ExtractVidPid(ReadString(item, "InstanceId"));
    device.description = ReadString(item, "Description");
    device.persisted_guid = ReadString(item, "PersistedGuid");
    device.stub_instance_id = ReadString(item, "StubInstanceId");
    device.client_ip = ReadString(item, "ClientIPAddress");
    device.is_forced = ReadBool(item, "IsForced");
    devices.push_back(std::move(device));
  }
  return devices;
}

std::vector<UsbDeviceDef> QueryState() {
  const auto result = platform::RunCommand(L"usbipd.exe state");

  if (!result.launched) {
    spdlog::error("failed to run usbipd state, error={}", result.error);
    return {};
  }
  if (result.exit_code != 0) {
    spdlog::warn("usbipd state exited with code {}", result.exit_code);
  }
  spdlog::debug("usbipd state output=\n{}", result.output);

  return ParseUsbipdState(result.output);
}

void usbipd_state() {
  const auto devices = QueryState();
  spdlog::info("usbipd state refreshed, {} device(s)", devices.size());
}

}  // namespace usbipd
