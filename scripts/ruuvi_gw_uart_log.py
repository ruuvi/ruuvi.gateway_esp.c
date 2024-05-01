#!/usr/bin/env python3

import argparse
import sys
import glob
import subprocess
import re
from datetime import datetime

description = """
This script is used to capture UART log output and save it to a file.
It is based on the 'screen' command.
To exit the screen session, press Ctrl+A followed by \,
or press Ctrl+A, then :quit
"""
parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)


# Function to print error messages in red
def error(message):
    color_red = '\033[0;31m'
    color_no = '\033[0m'  # No Color
    print(f"{color_red}Error: {message}{color_no}")


def print_usage():
    parser.print_help()


def parse_arguments():
    parser.add_argument('--port',
                        type=str,
                        default=None,
                        help='Serial Port')

    arguments = parser.parse_args()

    return arguments


def autodetect_serial_port():
    # Find ports
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/cu.wchusbserial*')

    # If no port available
    if not ports:
        error("No port available. Please connect a device.")
        sys.exit(1)

    # If more than one port available
    elif len(ports) > 1:
        error("Multiple ports available. Please specify the port.")
        print_usage()
        sys.exit(1)

    # Set the only available port
    else:
        serial_port = ports[0]
    print(f"Automatically detected serial port: {serial_port}")
    return serial_port


def remove_ansi_escape_sequences(file_path):
    ansi_escape = re.compile(r'\x1b\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]')
    with open(file_path, 'r') as file:
        lines = file.readlines()
    with open(file_path, 'w') as file:
        for line in lines:
            cleaned_line = ansi_escape.sub('', line)
            file.write(cleaned_line)


def main():
    arguments = parse_arguments()

    serial_port = arguments.port
    if not serial_port:
        serial_port = autodetect_serial_port()

    log_file_name = datetime.now().strftime("ruuvi_gw_log_%Y-%m-%dT%H-%M-%S.log")
    cmd_with_args = ['screen', '-L', '-Logfile', log_file_name, serial_port, '115200']
    try:
        print(' '.join(cmd_with_args))
        subprocess.check_call(cmd_with_args)
        remove_ansi_escape_sequences(log_file_name)
    except subprocess.CalledProcessError as e:
        error("'screen' execution failed.")
        sys.exit(e.returncode)


if __name__ == '__main__':
    main()
