#include "ReadFile.h"

#include <sstream>

namespace monitor {
/**
 * @brief         读取下一行并按空白字符拆分为字段
 *
 * @param         args 输出参数，保存拆分后的字段
 * @return        读取成功返回 true，否则返回 false
 */
bool ReadFile::ReadLine(std::vector<std::string> &args) {
    if (!file_.is_open()) return false;
    std::string line;
    std::getline(file_, line);
    if (file_.eof() || line.empty()) return false;

    std::istringstream iss(line);
    while (!iss.eof()) {
        std::string word;
        iss >> word;
        args.push_back(word);
    }
    return true;
}
} // namespace monitor
