#include "UserMonitor.h"

#include <unistd.h>
#include <sys/types.h>

#include <fstream>
#include <sstream>
#include <string>

#include "monitor_info.pb.h"

namespace monitor {

std::string UserMonitor::getUsernameByUid(uid_t uid) {
    std::ifstream file("/etc/passwd");
    if (!file.is_open()) return "";

    std::string line;
    while (std::getline(file, line)) {
        // /etc/passwd 格式: username:password:uid:gid:gecos:home:shell
        // 字段以冒号分隔
        std::istringstream iss(line);
        std::string username, password, uidStr;

        // 解析第一个字段：用户名
        if (!std::getline(iss, username, ':')) continue;

        // 解析第二个字段：密码（通常是 x）
        if (!std::getline(iss, password, ':')) continue;

        // 解析第三个字段：UID
        if (!std::getline(iss, uidStr, ':')) continue;

        // 将 UID 字符串转换为数字并比较
        try {
            uid_t parsedUid = static_cast<uid_t>(std::stoul(uidStr));
            if (parsedUid == uid) return username;
        } catch (const std::exception &) {
            // 解析失败，跳过此行
            continue;
        }
    }

    return "";
}

void UserMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo) {
    if (!monitorInfo) return;

    // 使用系统调用获取当前进程的实际用户ID
    uid_t uid = getuid();

    // 根据 UID 查找用户名
    std::string username = getUsernameByUid(uid);

    if (!username.empty()) monitorInfo->set_name(username);
}

} // namespace monitor
