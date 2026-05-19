#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace monitor {

/**
 * @brief         简单文件读取工具，按行读取文件并拆分为空白分隔的字段
 *
 */
class ReadFile {
public:
    /**
     * @brief         打开指定文件用于读取
     *
     * @param         name 文件路径
     */
    explicit ReadFile(const std::string &name) : file_(name) {}

    /**
     * @brief         析构时关闭已打开的文件
     *
     */
    ~ReadFile() {
        if (file_.is_open()) file_.close();
    }

    /**
     * @brief         读取下一行并按空白字符拆分为字段
     *
     * @param         args 输出参数，保存拆分后的字段
     * @return        读取成功返回 true，文件结束或读取失败返回 false
     */
    bool ReadLine(std::vector<std::string> &args);

private:
    std::ifstream file_;
};

} // namespace monitor
