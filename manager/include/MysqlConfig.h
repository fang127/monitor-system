#pragma once

#include <cstdlib>
#include <string>

namespace monitor {

struct MysqlConfig {
    std::string host;
    unsigned int port;
    std::string user;
    std::string password;
    std::string database;
};

inline std::string getEnvOrDefault(const char *name, const char *defaultValue) {
    const char *value = std::getenv(name);
    return (value && value[0] != '\0') ? value : defaultValue;
}

inline unsigned int getEnvPortOrDefault(const char *name,
                                        unsigned int defaultValue) {
    const char *value = std::getenv(name);
    if (!value || value[0] == '\0') return defaultValue;

    char *end = nullptr;
    unsigned long port = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0' || port == 0 || port > 65535)
        return defaultValue;

    return static_cast<unsigned int>(port);
}

inline MysqlConfig loadMysqlConfigFromEnv() {
    const char *password = std::getenv("MYSQL_PASSWORD");
    return MysqlConfig{
        getEnvOrDefault("MYSQL_HOST", "127.0.0.1"),
        getEnvPortOrDefault("MYSQL_PORT", 3306),
        getEnvOrDefault("MYSQL_USER", "root"),
        password ? password : "",
        getEnvOrDefault("MYSQL_DATABASE", "monitor-system"),
    };
}

} // namespace monitor
