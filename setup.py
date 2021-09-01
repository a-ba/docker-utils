#!/usr/bin/env python3

from setuptools import setup, find_packages, Extension
import sys

assert sys.version_info[0] > 2, "python2 not supported"

setup(
    name        = "docker-utils-aba",
    version     = "0.0.20",
    scripts     = """
        docker-diff
        docker-flatten
        docker-nsenter
        docker-remove-untagged-images
        docker-remove-zombies
        docker-rundev
        docker-runx
        docker-ssh
        docker-upgrade
    """.split(),

    packages    = ["docker_utils_aba"],
    package_data = {"docker_utils_aba": """
        docker-rundev-seccomp.json
        docker-upgrade-script.sh
    """.split()},
)
