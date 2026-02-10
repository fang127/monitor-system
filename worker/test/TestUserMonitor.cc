/**
 * @brief         Test User Monitor
 * @file          TestUserMonitor.cc
 * @author        harry
 * @date          2026-02-10
 */
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
/*!
 * @brief         Get the User Name object
 *
 * @param         uid
 * @return        std::string
 */
std::string getUserName(uid_t uid)
{
    std::ifstream passwdFile("/etc/passwd", std::ios::in);
    if (!passwdFile.is_open())
    {
        std::cerr << "Failed to open /etc/passwd" << std::endl;
        return "";
    }

    std::string line;
    while (std::getline(passwdFile, line))
    {
        std::istringstream ss(line);
        std::string userName, password, uidStr, gidStr, userInfo, homeDir,
            shell;
        if (!std::getline(ss, userName, ':')) continue;
        if (!std::getline(ss, password, ':')) continue;
        if (!std::getline(ss, uidStr, ':')) continue;
        if (!std::getline(ss, gidStr, ':')) continue;
        if (!std::getline(ss, userInfo, ':')) continue;
        if (!std::getline(ss, homeDir, ':')) continue;
        if (!std::getline(ss, shell, ':')) continue;

        try
        {
            uid_t parsedUid = static_cast<uid_t>(std::stoul(uidStr));
            if (parsedUid == uid)
            {
                return userName;
            }
        }
        catch (const std::exception &e)
        {
            continue;
        }
    }
    return "";
}
int main()
{
    uid_t uid = getuid();
    std::cout << "Current user ID: " << uid << std::endl;

    // 找到当前用户的用户名
    std::string userName = getUserName(uid);

    if (!userName.empty())
        std::cout << "User Name : " << userName << std::endl;
    else
        std::cout << "Failed to find user name for UID: " << uid << std::endl;

    // 对比环境变量
    const char *envUser = std::getenv("USER");
    if (envUser)
        std::cout << "Environment USER: " << envUser << std::endl;
    else
        std::cout << "Environment variable USER is not set." << std::endl;

    return 0;
}