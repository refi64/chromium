#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automatically creates a Flatpak bundle of a build."""

from __future__ import print_function

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys


try:
    from shlex import quote
except ImportError:
    from pipes import quote


RUNTIME_REPO = 'https://flathub.org/repo/flathub.flatpakrepo'
APP_ID = 'com.google.Chromium'


def write_paths_script(out_dir, paths_script):
    PATHS = [
        'chrome',
        'icudtl.dat',
        '*.so',
        '*.pak',
        '*.bin',
        '*.png',
        'locales',
        'MEIPreload',
    ]

    script = []

    script.append('BUILD_ROOT=%s' % quote(os.path.abspath(out_dir)))
    script.append('PATHS=(')

    for pattern in PATHS:
        for path in glob.iglob(os.path.join(out_dir, pattern)):
            script.append(os.path.basename(path))

    script.append(')')

    with open(paths_script, 'w') as fp:
        fp.write('\n'.join(script))


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('task', help='What to do',
                        choices=['shell', 'bundle', 'install'])
    parser.add_argument('out_dir', help='Directory to build a Flatpak for')
    parser.add_argument('--clean', help='Clean the OSTree repository and builder state first',
                        action='store_true')
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.realpath(__file__))
    manifest_file = os.path.join(script_dir, '%s.yaml' % APP_ID)
    krb_file = os.path.join(script_dir, 'krb5.conf')
    out_dir = args.out_dir
    flatpak_dir = os.path.join(out_dir, 'flatpak')
    manifest_copy = os.path.join(flatpak_dir, os.path.basename(manifest_file))
    paths_script = os.path.join(flatpak_dir, 'paths.sh')
    build_dir = os.path.join(flatpak_dir, 'build')
    repo_dir = os.path.join(flatpak_dir, 'repo')

    dirs_to_clean = [build_dir]
    if args.clean:
        dirs_to_clean.append(repo_dir)

    for dir_to_clean in filter(os.path.exists, dirs_to_clean):
        print('Cleaning up: %s' % dir_to_clean)
        shutil.rmtree(dir_to_clean)

    for required_dir in flatpak_dir, build_dir, repo_dir:
        if not os.path.exists(required_dir):
            os.mkdir(required_dir)

    print('Setting up Flatpak environment')
    shutil.copyfile(manifest_file, manifest_copy)
    shutil.copyfile(krb_file, os.path.join(flatpak_dir,
                                           os.path.basename(krb_file)))
    if args.task != 'shell':
        write_paths_script(out_dir, paths_script)

    print('Running flatpak-builder')

    builder_args = ['flatpak-builder', '--repo=%s' % repo_dir,
                    build_dir, manifest_copy]
    if args.task == 'shell':
        builder_args.append('--stop-at=chrome')
    elif args.task == 'install':
        builder_args.extend(['--install', '--user'])

    subprocess.check_call(builder_args)

    if args.task == 'shell':
        os.execvp('flatpak-builder', ['flatpak-builder', '--run',
                                      build_dir, manifest_copy, 'bash', '-c',
                                      '. /app/buildenv.sh; exec bash'])
    elif args.task == 'bundle':
        print('Creating bundle', APP_ID)
        subprocess.check_call(['flatpak', 'build-bundle',
                               '--runtime-repo=%s' % RUNTIME_REPO, repo_dir,
                               os.path.join(flatpak_dir, '%s.flatpak' % APP_ID),
                               APP_ID])


if __name__ == '__main__':
    sys.exit(main())
