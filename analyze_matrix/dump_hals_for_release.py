#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Dump new HIDL types that are introduced in each FCM version.
"""

from __future__ import print_function

import argparse
import collections
import json
import os
import subprocess
import sys

class Globals:
    pass

def call(args):
    if Globals.verbose:
        print(' '.join(args), file=sys.stderr)
    sp = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = sp.communicate()
    return sp.returncode, out.decode(), err.decode()

def check_call(args):
    r, o, e = call(args)
    assert not r, '`{}` returns {}'.format(' '.join(args), r)
    return o, e

def lines(o):
    return filter(lambda line: line, (line.strip() for line in o.split()))

def to_level(s):
    if s == 'legacy': return 0
    if s == '': return float('inf')
    return int(s)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verbose', help='Verbose mode', action='store_true')
    parser.add_argument('--pretty', help='Print pretty JSON', action='store_true')
    parser.add_argument('--hidl-gen', help='Location of hidl-gen', required=True)
    parser.add_argument('--analyze-matrix', help='Location of analyze_matrix', required=True)
    parser.add_argument('--compatibility-matrix', metavar='FILE',
        help='Location of framework compatibility matrices', nargs='+', required=True)
    parser.add_argument('--package-root', metavar='PACKAGE:PATH',
        help='package roots provided to hidl-gen, e.g. android.hardware:hardware/interfaces',
        nargs='+')
    parser.parse_args(namespace=Globals)

    interfaces_for_level = dict()
    for matrix_path in Globals.compatibility_matrix:
        r, o, _ = call([Globals.analyze_matrix, '--input', matrix_path, '--level'])
        if r:
            # Not a compatibility matrix, ignore
            continue
        # stderr may contain warning message if level is empty
        level = o.strip()

        o, _ = check_call([Globals.analyze_matrix, '--input', matrix_path, '--interfaces'])
        # stderr may contain warning message if no interfaces

        interfaces = list(lines(o))
        if not interfaces: continue

        if level not in interfaces_for_level:
            interfaces_for_level[level] = set()

        # Put top level interfaces
        interfaces_for_level[level].update(interfaces)

        # Put interfaces referenced by top level interfaces
        args = [Globals.hidl_gen, '-Ldependencies']
        if Globals.package_root:
            args.append('-R')
            for package_root in Globals.package_root:
                args.extend(['-r', package_root])
        args.extend(interfaces)
        o, e = check_call(args)
        assert not e, '`{}` has written to stderr:\n{}'.format(' '.join(args), e)
        interfaces_for_level[level].update(lines(o))

    seen_interfaces = set()
    new_interfaces_for_level = collections.OrderedDict()
    for level, interfaces in sorted(interfaces_for_level.items(), key=lambda tup: to_level(tup[0])):
        new_interfaces_for_level[level] = sorted(interfaces - seen_interfaces)
        seen_interfaces.update(interfaces)

    print(json.dumps(new_interfaces_for_level,
        separators=None if Globals.pretty else (',',':'),
        indent=4 if Globals.pretty else None))

if __name__ == '__main__':
    main()
