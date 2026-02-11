# this is a debug build script, it will not be used in production
# it will be used to build the project in debug mode, which will include debug symbols and will not be optimized
# it will also include the debug version of the libraries, which will be larger than the release version
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
    # conan install the dependencies
    cmd1 = "conan install . --build=missing --settings=build_type=Debug"
    if not runCommand(cmd1, "Failed to install dependencies with conan"):
        sys.exit(1)

    # cmake configure the project
    cmd2 = "cmake --preset conan-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    if not runCommand(cmd2, "Failed to configure the project with cmake"):
        sys.exit(1)

    # cmake build the project
    cmd3 = "cmake --build --preset conan-debug"
    if not runCommand(cmd3, "Failed to build the project with cmake"):
        sys.exit(1)

    print("Project built successfully in debug mode")
    sys.exit(0)

if __name__ == "__main__":
    main()
    