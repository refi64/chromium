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


# Right now this is using the 19.08beta runtime, this will switch to Flathub once the time
# comes.
RUNTIME_REPO = 'https://cache.sdk.freedesktop.org/freedesktop-sdk.flatpakrepo'
APP_ID = 'com.google.Chromium'


def reflink_copy(source, target):
    print('  Copy %s -> %s' % (source, target))
    subprocess.check_call(['cp', '-a', '--reflink=auto', source, target])


def install_chromium_build_files(out_dir, target_root):
    PATHS_TO_COPY = [
        'chrome',
        'icudtl.dat',
        '*.so',
        '*.pak',
        '*.bin',
        '*.png',
        'locales',
        'MEIPreload',
    ]

    chrome_dir = os.path.join(target_root, 'chrome')
    os.mkdir(chrome_dir)

    for pattern in PATHS_TO_COPY:
        for path in glob.iglob(os.path.join(out_dir, pattern)):
            reflink_copy(path, os.path.join(chrome_dir))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('out_dir', help='Directory to build a Flatpak for')
    parser.add_argument('--install_not_bundle', help='Directly install instead of a bundle',
                        action='store_true')
    parser.add_argument('--clean', help='Clean the OSTree repository and builder state first',
                        action='store_true')
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.realpath(__file__))
    manifest_file = os.path.join(script_dir, '%s.yaml' % APP_ID)
    out_dir = args.out_dir
    flatpak_dir = os.path.join(out_dir, 'flatpak')
    build_dir = os.path.join(flatpak_dir, 'build')
    repo_dir = os.path.join(flatpak_dir, 'repo')
    state_dir = os.path.join(flatpak_dir, 'state')

    dirs_to_clean = [build_dir]
    if args.clean:
        dirs_to_clean.append(repo_dir)
        dirs_to_clean.append(state_dir)

    for dir_to_clean in filter(os.path.exists, dirs_to_clean):
        print('Cleaning up: %s' % dir_to_clean)
        shutil.rmtree(dir_to_clean)

    for required_dir in flatpak_dir, build_dir, repo_dir:
        if not os.path.exists(required_dir):
            os.mkdir(required_dir)

    print('Initializing Flatpak build directory')
    subprocess.check_call(['flatpak-builder', '--force-clean', '--build-only',
                           '--state-dir=%s' % state_dir, build_dir, manifest_file])

    print('Installing built files')
    install_chromium_build_files(out_dir, os.path.join(build_dir, 'files'))

    print('Finalizing build')
    subprocess.check_call(['flatpak-builder', '--finish-only', '--state-dir=%s' % state_dir,
                           '--repo=%s' % repo_dir, build_dir, manifest_file]
                          + (['--install', '--user'] if args.install_not_bundle else []))

    if not args.install_not_bundle:
        for app_id in APP_ID, '%s.Debug' % APP_ID:
            print('Creating bundle', app_id)
            subprocess.check_call(['flatpak', 'build-bundle', '--runtime-repo=%s' % RUNTIME_REPO,
                                   repo_dir, os.path.join(flatpak_dir, '%s.flatpak' % APP_ID),
                                   APP_ID])


if __name__ == '__main__':
    sys.exit(main())
