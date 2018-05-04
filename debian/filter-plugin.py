#!/usr/bin/python3

# Inverse dh-exec-filter-profiles
# Author: Sebastian Ramacher <sramacher@debian.org>

import os
import re
import sys


remove_plugins = os.getenv("removeplugins")
if remove_plugins is not None:
    remove_plugins = set(remove_plugins.split(" "))
else:
    remove_plugins = set()

plugin_re = re.compile(r"^(\S*) \[([a-zA-Z1-9.,_-]*)\]$")


for line in sys.stdin.readlines():
    line = line.rstrip("\n")
    match = plugin_re.match(line)
    if not match:
        print(line)
        continue

    path = match.group(1)
    plugins = match.group(2)
    plugins = set(plugins.split(','))
    if not plugins & remove_plugins:
        print(path)
