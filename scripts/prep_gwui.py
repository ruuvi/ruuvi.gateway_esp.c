#!/usr/bin/python3

# @file prep_gwui.py
# @author TheSomeMan
# @date 2020-11-04
# @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.

import argparse
import os
import subprocess
import sys
import shutil
import gzip
from datetime import datetime


def main():
    parser = argparse.ArgumentParser(description='This tool prepares GWUI')
    parser.add_argument('-i', '--input', required=True, help='Input directory path')
    parser.add_argument('-o', '--output', required=True, help='Output directory path')
    args = parser.parse_args()
    input_dir = args.input
    output_dir = args.output
    if os.path.exists(output_dir):
        if os.path.isfile(output_dir):
            print('Error: %s is a file, but it should be a dir' % output_dir, file=sys.stderr)
            sys.exit(1)
        print('Remove dir: %s' % output_dir)
        shutil.rmtree(output_dir)
    print('Create dir: %s' % output_dir)
    os.mkdir(output_dir)
    timestamp = datetime(2000, 1, 1, 0, 0, 0).timestamp()
    for root, dirs, files in os.walk(input_dir):
        rel_root = os.path.relpath(root, start=input_dir)
        target_dir = os.path.join(output_dir, rel_root)
        for dir_name in dirs:
            os.mkdir(os.path.join(target_dir, dir_name))
        for file in files:
            file_ext = os.path.splitext(file)[1]
            if file_ext == '.js' or file_ext == '.html' or file_ext == '.css':
                src_file = os.path.join(root, file)
                with open(src_file, 'rb') as f_in:
                    dst_file = os.path.join(target_dir, file + '.gz')
                    print('%s -> %s' % (src_file, dst_file))
                    with gzip.GzipFile(filename=dst_file, mode='wb', mtime=timestamp) as f_out:
                        shutil.copyfileobj(f_in, f_out)
            elif file_ext == '.ico' or file_ext == '.woff' or file_ext == '.woff2':
                src_file = os.path.join(root, file)
                dst_file = os.path.join(target_dir, file)
                print('%s -> %s' % (src_file, dst_file))
                shutil.copy(src_file, dst_file)
            else:
                src_file = os.path.join(root, file)
                print('Skip %s' % src_file)


if __name__ == '__main__':
    main()
