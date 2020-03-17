#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Synchronizes the Chromium Flatpak bundles  with a GCP bucket."""

from __future__ import print_function

import argparse
import os
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('bucket_path', help='The GCP Cloud Storage bucket to sync with')
    parser.add_argument('repo', help='The local directory to sync the repo into')
    parser.add_argument('bundles', help='The Flatpak bundles to import', action='append',
                        default=[])
    parser.add_argument('--gpg', help='The gpg key to sign with')
    parser.add_argument('--gpgdir', help='The gpg homedir')
    parser.add_argument('--init_empty_subdir', help='Automatically initialize an empty subdir',
                        action='store_true')
    args = parser.parse_args()

    repo = args.repo
    if not os.path.exists(args.repo):
        os.makedirs(args.repo)

    flatpak_gpg_args = []
    if args.gpg is not None:
        flatpak_gpg_args.append('--gpg-sign=%s' % args.gpg)
    if args.gpgdir is not None:
        flatpak_gpg_args.append('--gpg-homedir=%s' % args.gpgdir)

    bucket_uri = 'gs://%s' % args.bucket_path
    placeholder_uri = '%s/placeholder' % bucket_uri

    if args.init_empty_subdir:
        print('Initializing...')

        with tempfile.NamedTemporaryFile() as temp:
            placeholder = '%s/placeholder' % bucket_uri
            subprocess.check_call(['gsutil', 'cp', temp.name, placeholder_uri])

    print('Downsync...')

    subprocess.check_call(['gsutil', '-m', 'rsync', '-rd', bucket_uri, repo])
    subprocess.check_call(['ostree', 'init', '--mode=archive', '--repo=%s' % repo])

    print('Import...')
    for bundle in args.bundles:
        subprocess.check_call(['flatpak', 'build-import-bundle', repo, bundle] + flatpak_gpg_args)

    if flatpak_gpg_args:
        subprocess.check_call(['flatpak', 'build-sign', repo] + flatpak_gpg_args)
        subprocess.check_call(['flatpak', 'build-update-repo', repo] + flatpak_gpg_args)

    print('Upsync...')

    subprocess.check_call(['gsutil', '-m', 'rsync', '-rd', repo, bucket_uri])

    if args.init_empty_subdir:
        subprocess.check_call(['gsutil', 'rm', placeholder_uri])


if __name__ == '__main__':
    main()
