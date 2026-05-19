from conan import ConanFile
from conan.tools.cmake import cmake_layout


class MonitorSystemConan(ConanFile):
     # settings是ConanFile的一个属性，用于定义包的构建环境相关的设置项。这里定义了四个设置项：操作系统（os）、编译器（compiler）、构建类型（build_type）和架构（arch）。这些设置项将用于指定包在不同环境下的构建方式和依赖关系。
    settings = "os", "compiler", "build_type", "arch"
    # generators是ConanFile的一个属性，用于指定生成器（generator）的类型。生成器是Conan用来生成构建系统文件的工具。在这个例子中，使用了两个生成器：CMakeDeps和CMakeToolchain。CMakeDeps生成器用于生成CMake的依赖文件，而CMakeToolchain生成器用于生成CMake的工具链文件。这些生成器将帮助构建系统正确地找到和链接所需的依赖项。
    generators = "CMakeDeps", "CMakeToolchain"

    # requirements方法是ConanFile的一个方法，用于定义包的依赖关系。在这个方法中，使用self.requires()方法来指定包所依赖的其他包。每个调用self.requires()都会添加一个依赖项到当前包的依赖列表中。在这个例子中，定义了五个依赖项：grpc、libmysqlclient、libbpf、folly和redis-plus-plus。此外，还使用了override=True参数来解决folly依赖的lz4版本与现有图中的lz4版本冲突的问题。
    def requirements(self):
        self.requires("grpc/1.72.0")
        self.requires("libmysqlclient/8.1.0")
        self.requires("libbpf/1.3.0")
        self.requires("folly/2024.08.12.00")
        self.requires("redis-plus-plus/1.3.15")

        # 解决 folly 依赖的 lz4/1.10.0 与现有依赖图中的 lz4/1.9.4 冲突。
        self.requires("lz4/1.10.0", override=True)
        self.requires("zstd/1.5.7", override=True)

    # layout方法是ConanFile的一个方法，用于定义包的布局。在这个方法中，使用cmake_layout()函数来设置CMake的布局方式。cmake_layout()函数会根据Conan的设置项自动生成适合CMake构建系统的目录结构和文件路径。这将帮助构建系统正确地找到源代码、构建文件和生成的二进制文件。
    def layout(self):
        cmake_layout(self)
