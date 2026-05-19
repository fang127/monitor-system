# 这是调试构建脚本，不用于生产环境。
# 它会以 Debug 模式构建项目，包含调试符号且不会开启优化。
# 它也会链接调试版本的库，因此构建产物通常比 Release 版本更大。
import subprocess
import sys

def runCommand(command, errorMessage):
    print(f"Running command: {command}")
    result = subprocess.run(command, shell=True)
    if result.returncode != 0:
        print(errorMessage)
        return False
    return True

def main():
    # 使用 conan 安装依赖
    cmd1 = "conan install . --build=missing --settings=build_type=Debug"
    if not runCommand(cmd1, "Failed to install dependencies with conan"):
        sys.exit(1)

    # 使用 cmake 配置项目
    cmd2 = "cmake --preset conan-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    if not runCommand(cmd2, "Failed to configure the project with cmake"):
        sys.exit(1)

    # 使用 cmake 构建项目
    cmd3 = "cmake --build --preset conan-debug"
    if not runCommand(cmd3, "Failed to build the project with cmake"):
        sys.exit(1)

    print("Project built successfully in debug mode")
    sys.exit(0)

if __name__ == "__main__":
    main()
