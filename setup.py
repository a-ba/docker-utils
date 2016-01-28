#!/usr/bin/env python3

from setuptools import setup, find_packages, Extension
import sys

assert sys.version_info[0] > 2, "python2 not supported"

setup(
    name        = "docker-utils-aba",
    version     = "0.0.1",
    scripts     = """
        docker-diff
        docker-remove-untagged-images
        docker-remove-zombies
        docker-rundev
        docker-runx
        docker-ssh
    """.split()
)
