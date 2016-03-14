#!/usr/bin/python3

import os
import re
import sys
import subprocess

import pkg_resources

def die(fmt, *k):
    sys.stderr.write("error: %s\n" % (fmt % k))
    sys.exit(1)

def docker_version():
    out = subprocess.check_output(["docker", "-v"])
    mo = re.match(br"Docker version (\d+)\.(\d+)\.(\d+)", out)
    if mo:
        return tuple(map(int, mo.groups()))
    die("unable to parse a version number from the output of 'docker -v'")


def pkg_file(path):
    return pkg_resources.resource_filename(
            pkg_resources.Requirement.parse("docker-utils-aba"),
            os.path.join("docker_utils_aba", path))
