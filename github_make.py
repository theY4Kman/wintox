import errno
import os
import subprocess
import getpass
from tempfile import NamedTemporaryFile
from pygithub3 import Github
from threading import Thread

SPCOMP = '../spcomp'
SPCOMP_LONGLINES = './spcomp_longlines'


class BuildError(Exception):
    pass


class BuildDownloadProcess(object):

    def __init__(self, gametype, debug=False, compiler=SPCOMP_LONGLINES,
                 include='../include', github=None, user=None, repo=None,
                 outdir='build/tmp/'):
        self.gametype = gametype
        self.debug = debug

        self.compiler_path = compiler
        self.include = include
        self.outdir = outdir

        self.github = github
        self.user = user
        self.repo = repo

    @property
    def compiler(self):
        if hasattr(self, '_compiler'):
            return self._compiler
        self._compiler = os.path.basename(self.compiler_path)
        return self._compiler

    @property
    def out(self):
        if hasattr(self, '_out'):
            return self._out

        try:
            os.makedirs(self.outdir)
        except OSError, e:
            if e.errno != errno.EEXIST:
                raise

        self._out = NamedTemporaryFile(dir=self.outdir,
                                       prefix=self.get_plugin_basename(),
                                       suffix='.smx', delete=False)
        self._out.close()
        os.chmod(self._out.name, 0777)

        return self._out

    def get_plugin_filename(self):
        return self.get_plugin_basename() + '.smx'

    def get_plugin_basename(self):
        return 'wintox_%s%s' % (self.gametype,
                                '_debug' if self.debug else '')

    def get_source_filename(self):
        return 'wintox_%s.sp' % self.gametype

    def get_plugin_commandline(self, outpath=None):
        if outpath is None:
            outpath = self.get_plugin_basename()

        options = [
            self.compiler,
            '-o' + outpath,
            '-i' + self.include
        ]
        if self.debug:
            options.append('WINTOX_DEBUG=1')

        options.append(self.get_source_filename())
        return options

    def build_plugin(self):
        options = self.get_plugin_commandline(self.out.name)
        try:
            output = subprocess.check_output(options, stderr=subprocess.PIPE)
        except subprocess.CalledProcessError, e:
            raise BuildError('%s exited with error code %d (cmd: `%s`)' %
                             (self.compiler, e.returncode, ' '.join(options)))

        return self.out

    def remove_old_download(self):
        """
        Removes an older version of the download to make room for the newly
        compiled plug-in. Returns True if the download was found, or False if
        it wasn't.
        """

        if not self.github:
            return

        filename = self.get_plugin_filename().lower()
        for page in self.github.repos.downloads.list(user=self.user,
                                                     repo=self.repo):
            for dl in page:
                if dl.name.lower() == filename:
                    self.github.repos.downloads.delete(dl.id, user=self.user,
                                                       repo=self.repo)
                    return True

        return False

    def create_download(self):
        if not self.github:
            return

        return self.github.repos.downloads.create({
            'name': self.get_plugin_filename(),
            'size': os.stat(self.out.name).st_size
        }, user=self.user, repo=self.repo)

    def run(self, output=True, job=True):
        out = lambda o: None
        if output:
            def out(obj):
                print obj

        if job:
            print 'Starting %s' % self.get_plugin_basename()

        out('Building %s plug-in...' % self.get_plugin_basename())
        self.build_plugin()
        out('Built!')

        out('Removing old download...')
        self.remove_old_download()
        out('Removed!')

        out('Creating resource...')
        download = self.create_download()
        out('Created!')

        out('Uploading file...')
        download.upload(self.out.name)
        out('Uploaded!')

        if job:
            print 'Completed %s' % self.get_plugin_basename()


def main(argv=None):
    if argv is None:
        import sys
        argv = sys.argv

    password = getpass.getpass('Github Password (enter to quit): ')
    if not password:
        return
    
    github = Github(login='theY4Kman', password=password)

    for debug in (True, False):
        for gametype in ('surf', 'bhop'):
            def run():
                build = BuildDownloadProcess(gametype, debug=debug, github=github,
                                             user='theY4Kman', repo='wintox')
                build.run(output=False, job=True)

            thread = Thread(target=run)
            thread.start()


if __name__ == '__main__':
    import sys
    main(sys.argv)
