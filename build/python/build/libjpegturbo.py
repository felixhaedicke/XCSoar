import subprocess

from build.autotools import AutotoolsProject

class LibJpegTurboProject(AutotoolsProject):
    def get_make_args(self, toolchain):
        base_make_args = AutotoolsProject.get_make_args(self, toolchain)

        ldflags = (toolchain.ldflags + ' ' + self.ldflags)
        if '-static' in ldflags:
            ldflags = ldflags.replace('-static', '-all-static')
            return base_make_args + ['LDFLAGS=' + ldflags]
        else:
            return base_make_args
