#!/usr/bin/env python3

from datetime import date
from os import system
from packaging.version import Version
from shutil import which
from subprocess import run, CalledProcessError

def check_gen_sidoc_version(version):
    try:
        p = run(["gen-sidoc", "--version"], capture_output=True)        
        return p.returncode == 0 and Version(p.stdout.decode()) >= Version(version)
    except CalledProcessError:
        return False

def main():
    hostname = 'CSH'
    docdate = str(date.today())
    if not which('gen-sidoc') or not check_gen_sidoc_version('0.1.11'):
        system("python3 -m pip install -U git+https://github.com/spaceinventor/libdoc.git")
    cmd_line = f"python3 -m libdoc -d {docdate} -t MAN -n 001 {hostname} -o build-doc/{hostname}_MAN.pdf doc/index_man.rst"
    print(cmd_line)
    system(cmd_line)


main()