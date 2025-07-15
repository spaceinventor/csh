#!/usr/bin/env python3

from datetime import date
from os import system, environ
from subprocess import run

def check_gen_sidoc_version(version):
    try:
        p = run(["python3", "-m", "libdoc", "--version"], capture_output=True)
        # Poor man's version comparison
        required_version = int(''.join([x for x in version if x.isdigit()]))
        current_version = int(''.join([x for x in p.stdout.decode() if x.isdigit()]))
        return p.returncode == 0 and current_version >= required_version
    except Exception:
        return False

def main():
    hostname = 'CSH'
    docdate = str(date.today())
    libdoc_version = "0.1.22"
    if not check_gen_sidoc_version(environ.get("LIBDOC_VERSION", libdoc_version)):
        if environ.get('SI_ORG_BUILD_USER_TOKEN', None):
            cmd = f"python3 -m pip install -U git+https://${environ.get('SI_ORG_BUILD_USER_TOKEN')}@github.com/spaceinventor/libdoc.git@{libdoc_version}"
        else:
            cmd = f"python3 -m pip install -U git+https://github.com/spaceinventor/libdoc.git@{libdoc_version}"
        print(cmd)
        system(cmd)
    cmd_line = f"python3 -m libdoc -d {docdate} -t MAN -n 001 {hostname} -o build-doc/{hostname}_MAN.pdf doc/index_man.rst"
    print(cmd_line)
    system(cmd_line)


main()