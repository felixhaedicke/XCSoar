import subprocess

from build.project import Project

class MakeProject(Project):
    def __init__(self, url, alternative_url, md5, installed,
                 install_target='install',
                 **kwargs):
        Project.__init__(self, url, alternative_url, md5, installed, **kwargs)
        self.install_target = install_target

    def get_simultaneous_jobs(self):
        return 12

    def get_make_args(self, toolchain):
        return ['--quiet', '-j' + str(self.get_simultaneous_jobs())]

    def get_make_install_args(self, toolchain):
        return ['--quiet', self.install_target]

    def build(self, toolchain, wd, install=True):
        subprocess.check_call(['/usr/bin/make'] + self.get_make_args(toolchain),
                              cwd=wd, env=toolchain.env)
        if install:
            subprocess.check_call(['/usr/bin/make'] + self.get_make_install_args(toolchain),
                                  cwd=wd, env=toolchain.env)
