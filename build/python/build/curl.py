from build.autotools import AutotoolsProject

class CurlProject(AutotoolsProject):
    def configure(self, toolchain):
        # disable usage of pthreads for Win32 builds
        if 'mingw' in toolchain.arch:
            self.configure_args.append('--enable-pthreads=no')

        return AutotoolsProject.configure(self, toolchain)

    def get_make_args(self, toolchain):
        base_make_args = AutotoolsProject.get_make_args(self, toolchain)

        ldflags = (toolchain.ldflags + ' ' + self.ldflags)
        if '-static' in ldflags:
            return base_make_args + ['curl_LDFLAGS=-all-static']
        else:
            return base_make_args
