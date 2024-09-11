#!/usr/bin/env python3
import os

def main():
    return os.system("valgrind -s --leak-check=full --show-leak-kinds=all builddir/csh -i tests/valgrind_test_csh.ini")

main()