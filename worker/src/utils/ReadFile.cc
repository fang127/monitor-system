#include "ReadFile.h"

#include <sstream>

namespace monitor {
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