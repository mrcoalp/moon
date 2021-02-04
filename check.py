#!/usr/bin/python3

import sys
import subprocess

args = sys.argv

if sys.platform == "linux" or sys.platform == "linux2":
    args[0] = "platform-scripts/linux/check.sh"
else:
    sys.exit()

subprocess.call(args, cwd=".")
