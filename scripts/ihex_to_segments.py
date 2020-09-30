#!/usr/bin/python3

# @file ihex_to_segments.py
# @author TheSomeMan
# @date 2020-09-15
# @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.

import argparse
import os
import sys
import bincopy
import binascii
import shutil


def main():
    parser = argparse.ArgumentParser(description='This tool converts Intel-hex file to a set of binary segments')
    parser.add_argument('-i', '--input', required=True, help='Input file in Intel-hex format')
    parser.add_argument('-o', '--output', required=True, help='Output directory path')
    parser.add_argument('-f', '--info', required=True, help='Output info file name')
    parser.add_argument('-v', '--fw_version', required=True, help='Firmware version')
    args = parser.parse_args()
    input_hex = args.input
    output_dir = args.output
    info_file_name = args.info
    fw_version = args.fw_version
    if os.path.exists(output_dir):
        if os.path.isfile(output_dir):
            print('Error: %s is a file, but it should be a dir' % output_dir, file=sys.stderr)
            sys.exit(1)
        print('Remove dir: %s' % output_dir)
        shutil.rmtree(output_dir)
    print('Create dir: %s' % output_dir)
    os.mkdir(output_dir)

    f = bincopy.BinFile()
    f.add_ihex_file(input_hex)
    print('Create: %s/%s' % (output_dir, info_file_name))
    with open('%s/%s' % (output_dir, info_file_name), 'w') as fd_info:
        fd_info.write('# %s\n' % fw_version)
        segment_num = 0
        for x in f.segments:
            segment_num += 1
            file_name = 'segment_%d.bin' % segment_num
            print('Segment %d: address 0x%08x, len %d -> %s/%s' %
                  (segment_num, x.address, len(x.data), output_dir, file_name))
            x_crc = binascii.crc32(x.data)
            fd_info.write('0x%08x %d %s 0x%08x\n' % (x.address, len(x.data), file_name, x_crc))
            with open('%s/%s' % (output_dir, file_name), 'wb') as fd_file:
                fd_file.write(x.data)


if __name__ == '__main__':
    main()
