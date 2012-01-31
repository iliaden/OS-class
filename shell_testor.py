#!/usr/bin/python

# This is the python test script for the 1st programming assignment of COMP
# 310, winter 2012.
#
# We are going to use this script to grade your shell.
# 
# Requirement:
#   This script requires Python 2.5 or higher version
#
# Author : Xing Shi Cai <xingshi.cai@mail.mcgill.ca>

import sys
import os
from subprocess import Popen, PIPE
import unittest

sh = ''

sys.stderr = sys.stdout

class TestYourShell(unittest.TestCase):
    
    def setUp(self):
        env = os.environ
        env['FAV_TV'] = 'Dr. House'
        self.sh = Popen([sh], 
                stdin=PIPE,
                stdout=PIPE,
                stderr=PIPE,
                env=env)

    echo_commands = '''echo aaa
    echo bbb ccc
    echo ddd eee fff
    '''

    def test_echo(self):
        '''
        Just test basic echo functions.
        '''
        self.out, self.err = self.sh.communicate(self.echo_commands)

        self.assert_success()

        self.assertEqual(self.out, 'aaa\nbbb ccc\nddd eee fff\n')

    history_command = 'set -o history\n' + echo_commands * 5 + 'history\n'
    def test_history(self):
        # I hope your shell could remember at least 15 commands
        self.out, self.err = self.sh.communicate(self.history_command)
        self.assert_success()

        history = self.out.split('\n')[-17:-1]
        for cmd in history[:15]:
            self.assert_(cmd.find('echo') >= 0, 
                    "history output should contain 'echo ...', but is '%s'" %
                    cmd)
        self.assert_(history[-1].endswith('history'),
                'The last command is not \'history\'')

    set_command = 'unset boss\n'
    def test_unset(self):
        self.out, self.err = self.sh.communicate(self.set_command)
        self.assert_success()

        self.assertEqual(self.out, '', 'unset should output nothing')

    env_command = 'env\n'
    def test_env(self):
        self.out, self.err = self.sh.communicate(self.env_command)
        self.assert_success()

        self.assert_(self.out.find("FAV_TV=Dr. House") >= 0,
                "'env' should print out all environment variables but at least one such variable is missing")

    cd_command = '''cd /etc
    pwd
    cd /var
    pwd
    '''
    def test_cd_pwd(self):
        self.out, self.err = self.sh.communicate(self.cd_command)
        self.assert_success()

        self.assertEqual(self.out, "/etc\n/var\n", 
                "Check what these commands do in bash - http://pastebin.com/Nvq81s1s")

    pushd_command = '''pushd /etc
    pushd /tmp
    pushd /var
    pushd /bin
    popd
    popd
    pwd
    '''
    def test_pushd_popd(self):
        self.out, self.err = self.sh.communicate(self.pushd_command)
        self.assert_success()

        self.assert_(self.out.endswith('/tmp\n'), 
                "Check what these commands do in bash - http://pastebin.com/KLxARn8X")

    redirect_out_command = 'ls > ls.out\n'
    def test_redirect_out(self):
        self.out, self.err = self.sh.communicate(self.redirect_out_command)
        self.assert_success()

        self.assertEqual(self.out, "", "'%s' shouldn't have output" %
                self.redirect_out_command)

        f = open('./ls.out', 'rb')
        content = f.read()
        self.assert_(content.find("shell_testor.py") >= 0, 
                "check ls.out in current directory.  It should contains output of 'ls'")
        f.close()

    redirect_in_command = 'wc < wc.in\n'
    def test_redirect_in(self):
        self.crete_wc_file()

        self.out, self.err = self.sh.communicate(self.redirect_in_command)
        self.assert_success()

        self.assertEqual(self.out, " 0  3 25\n", "< symbol doesn't work")

    pipe_command = 'cat ./wc.in | wc\n'
    def test_pipe(self):
        self.crete_wc_file()

        self.out, self.err = self.sh.communicate(self.pipe_command)
        self.assert_success()

        self.assertEqual(self.out, "      0       3      25\n", "| symbol doesn't work")

    def crete_wc_file(self):
        f = open('./wc.in', 'w')
        f.write('computer operating system')
        f.close()
        
    exit_command = history_command + set_command + \
            pushd_command + redirect_out_command + \
            env_command + cd_command + \
            'exit\n'
    def test_exit(self):
        self.out, self.err = self.sh.communicate(self.exit_command)
        self.assert_success()

    def assert_success(self):
        '''
        Do not test exit code any more.
        '''
        pass
        #self.assertEqual(self.sh.returncode, 0, 'Exit code of your shell is not 0')
        #self.assertEqual(self.err, '', 
        #        'Error output is not empty: %s' % self.err)


def main():
    global sh
    if len(sys.argv) < 2:
        print "Usage: python shell_testor.py [the executable file of your shell]"
        print "       e.g., 'python shell_testor.py bash'"
        return

    print "=" * 80
    print """This script expects your shell to behave in the same way as bash.  If you think
your shell works correctly, but this script doesn't agree, it's very likely
that your shell treats a command differently than bash does."""
    print "=" * 80

    # Not a good way to pass sh to test case, but this is just an assignment.
    sh = sys.argv[1]

    if sh != 'bash':
        if sh.find('/') < 0 and sh.find('\\') < 0:
            sh = os.path.join(os.getcwd(), sh)

        if not os.path.exists(sh):
            print "WARNING:"
            print "Can not find %s anywhere, please give the relative or absolute path to your shell" \
                % sys.argv[1]
            return

    suite = unittest.TestLoader().loadTestsFromTestCase(TestYourShell)
    unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite)

if __name__ == "__main__":
    main()
