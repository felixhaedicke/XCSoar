import subprocess
import tempfile

from build.makeproject import MakeProject

class CMakeProject(MakeProject):
    def __init__(self, url, alternative_url, md5, installed, configure_args=[],
                 cppflags='',
                 ldflags='',
                 libs='',
                 install_prefix=None,
                 install_target='install',
                 **kwargs):
        MakeProject.__init__(self, url, alternative_url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.cppflags = cppflags
        self.ldflags = ldflags
        self.libs = libs
        self.install_prefix = install_prefix
        self.install_target = install_target

    def configure(self, toolchain):
        src = self.unpack(toolchain)

        build = self.make_build_path(toolchain)

        install_prefix = self.install_prefix
        if install_prefix is None:
            install_prefix = toolchain.install_prefix

        toolchain_file = tempfile.NamedTemporaryFile('wt', delete=False)
        try:
            arch_splitted = toolchain.arch.rsplit('-')
            cmake_processor = arch_splitted[0];

            cmake_system_name = None
            if 'linux' in arch_splitted:
                cmake_system_name = 'Linux'
            elif 'darwin' in arch_splitted:
                cmake_system_name = 'Darwin'
            elif any("mingw" in s for s in arch_splitted):
                cmake_system_name = 'Windows'

            if cmake_system_name is not None:
                toolchain_file.write('SET(CMAKE_SYSTEM_NAME ' +
                    cmake_system_name + ')\n')

            toolchain_file.write('set(CMAKE_SYSTEM_PROCESSOR ' +
                    cmake_processor + ')\n')
            toolchain_file.write('set(CMAKE_C_COMPILER ' +
                    toolchain.cc + ')\n')
            toolchain_file.write('set(CMAKE_CXX_COMPILER ' +
                    toolchain.cxx + ')\n')
            toolchain_file.write('set(DCMAKE_AR ' + toolchain.ar + ')\n')
            toolchain_file.write('set(DCMAKE_RANLIB ' +
                    toolchain.ranlib + ')\n')
            toolchain_file.flush()

            toolchain_file.close()

            cmake_config = [
                'cmake',
                src,
                '-DCMAKE_TOOLCHAIN_FILE=' + toolchain_file.name,
                '-DCMAKE_C_FLAGS=' + toolchain.cppflags + ' ' + self.cppflags +
                        ' ' + toolchain.cflags,
                '-DCMAKE_CXX_FLAGS=' + toolchain.cppflags + ' ' +
                        self.cppflags + ' ' + toolchain.cxxflags,
                '-DCMAKE_EXE_LINKER_FLAGS=' + toolchain.ldflags +
                        ' ' + self.ldflags + ' ' + toolchain.libs +
                        ' ' + self.libs,
                '-DCMAKE_INSTALL_PREFIX=' + install_prefix,
            ] + self.configure_args

            subprocess.check_call(cmake_config, cwd=build, env=toolchain.env)

        finally:
            try:
                os.remove(toolchain_file.name)
            except:
                pass

        return build

    def build(self, toolchain):
        build = self.configure(toolchain)

        MakeProject.build(self, toolchain, build)
