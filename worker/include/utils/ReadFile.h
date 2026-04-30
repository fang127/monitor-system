#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace monitor {

class ReadFile {
public:
    explicit ReadFile(const std::string &name) : file_(name) {}

    ~ReadFile() {
        if (file_.is_open()) file_.close();
    }

    bool ReadLine(std::vector<std::string> &args);

private:
    std::ifstream file_;
};

} // namespace monitor