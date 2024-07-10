#!/usr/bin/env python3

# -----------------------------------------------------------------------------------
# File: ruuvi_gw_log.py
# Copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
# Author: TheSomeMan
#
# Description: This script is specifically designed for capturing logs from the USB serial port of a Ruuvi Gateway.
# -----------------------------------------------------------------------------------

import glob
import argparse
import subprocess
import sys
import os
import re
from datetime import datetime
import logging
import platform
import signal

requirements = ['pyserial']

description = """
This script is specifically designed for capturing logs from the USB serial port of a Ruuvi Gateway.

Key Features:

1. Automatic Detection of Serial Port: 
If not explicitly specified, the script will intuitively 
detect the only available USB serial port on its own.

2. User Prompt for Multiple Ports: 
In the event of multiple available USB serial ports and none 
being directly specified, the script will prompt the user 
to provide the specific port they intend to use.

Also it is possible to specify the serial port in the environment variable RUUVI_GW_SERIAL_PORT:
`export RUUVI_GW_SERIAL_PORT=/dev/ttyUSB1`
"""
parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

g_flag_disable_user_interaction = False


# This custom filter will allow through any records with a level less than ERROR
class StdoutFilter(logging.Filter):
    def filter(self, record):
        return record.levelno < logging.ERROR


class Logger:
    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.INFO)

        formatter = logging.Formatter('[%(asctime)s.%(msecs)03d %(levelname)s] %(message)s',
                                      datefmt='%Y-%m-%dT%H:%M:%S')

        handler_out = logging.StreamHandler(sys.stdout)
        handler_out.setLevel(logging.INFO)
        handler_out.setFormatter(formatter)
        handler_out.addFilter(StdoutFilter())

        handler_err = logging.StreamHandler(sys.stderr)
        handler_err.setLevel(logging.ERROR)
        handler_err.setFormatter(formatter)

        self.logger.addHandler(handler_out)
        self.logger.addHandler(handler_err)

    def addHandler(self, handler):
        self.logger.addHandler(handler)

    def debug(self, msg):
        self.logger.debug(msg)

    def info(self, msg):
        self.logger.info(msg)

    def warning(self, msg):
        self.logger.warning(msg)

    def error(self, msg):
        self.logger.error(msg)

    def critical(self, msg):
        self.logger.critical(msg)


logger = Logger()


# Function to print error messages in red
def error(message):
    logger.error(message)
    color_red = '\033[0;31;1m'
    color_no = '\033[0m'  # No Color
    print(f"{color_red}Error: {message}{color_no}", file=sys.stderr)


def signal_handler(sig, frame):
    error("CTRL+C detected. Exiting...")
    sys.exit(3)


signal.signal(signal.SIGINT, signal_handler)


def print_usage():
    parser.print_help()


def ask_user_to_continue():
    if g_flag_disable_user_interaction:
        return False
    print("Do you want to continue Y(es)/N(no)? ", end="", flush=True)

    def wait_for_keypress_windows():
        import msvcrt
        while True:
            if msvcrt.kbhit():
                key = msvcrt.getch()
                if key in [b'y', b'Y', b'n', b'N']:
                    return key.lower() == b'y'
                else:
                    print("\nInvalid input. Please enter Y for yes or N for no.")

    def wait_for_keypress_unix():
        import termios
        import tty
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        while True:
            tty.setraw(fd)
            try:
                ch = sys.stdin.read(1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
            if ch == '\x03':  # Control+C
                print("\nCTRL+C detected. Exiting...")
                sys.exit(1)
            if ch in ['y', 'Y', 'n', 'N']:
                return ch.lower() == 'y'
            else:
                print(f"\nInvalid input. Please enter Y for yes or N for no.")

    def wait_for_keypress_portable():
        while True:
            key = input()
            if key in ['y', 'Y', 'n', 'N']:
                return key.lower() == 'y'
            else:
                print("\nInvalid input. Please enter Y for yes or N for no.")

    os_name = platform.system()
    if os_name == 'Windows':
        wait_for_keypress = wait_for_keypress_windows
    elif os_name in ['Linux', 'Darwin']:  # 'Darwin' is for MacOS
        wait_for_keypress = wait_for_keypress_unix
    else:
        wait_for_keypress = wait_for_keypress_portable

    flag_continue = wait_for_keypress()
    print("")
    return flag_continue


def parse_arguments():
    parser.add_argument('--port',
                        type=str,
                        default=None,
                        help='Serial Port')

    parser.add_argument('--print_port',
                        action='store_true',
                        help='Print the only available serial port and exit.')

    parser.add_argument('--log_dir',
                        type=str,
                        default='.',
                        help='Directory to save log files.')

    arguments = parser.parse_args()

    return arguments


def available_serial_ports():
    import serial.tools.list_ports
    list1 = []
    list2 = []
    if platform.system() == 'Linux':
        list2.extend(glob.glob('/dev/ttyUSB*'))
    elif platform.system() == 'Darwin':
        # list2.extend(glob.glob('/dev/cu.wchusbserial*'))
        # list2.extend(glob.glob('/dev/cu.usbserial*'))
        list2.extend(glob.glob('/dev/tty.usbserial-*'))
    else:
        list1 = list(p.device for p in serial.tools.list_ports.comports())
    unique_list = list(set(list1 + list2))
    return list(reversed(sorted(unique_list)))


def autodetect_serial_port(ports):
    # If no port available
    if len(ports) == 0:
        error("No port available. Please connect a device.")
        sys.exit(1)

    # If more than one port available
    if len(ports) > 1:
        error(f"Multiple ports available: {ports}. Please specify the port.")
        sys.exit(1)

    # Set the only available port
    serial_port = ports[0]
    if not g_flag_disable_user_interaction:
        logger.info(f"Automatically detected serial port: {serial_port}")
    return serial_port


def log_serial_data(serial_port_name, output_file_name, console_output=False):
    import serial
    ansi_escape = re.compile(r'\x1b\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]')
    try:
        with serial.Serial(serial_port_name, baudrate=115200, timeout=1) as ser:
            with open(output_file_name, 'w') as f:
                logger.info(f"Logging UART data to {output_file_name}.")
                print(f"Press 'Ctrl+C' to stop.")
                print(f"")
                while True:
                    line = ser.readline()
                    if not line:
                        continue
                    line = line.decode('utf-8', errors='ignore').strip()
                    if console_output:
                        print(line)
                    cleaned_line = ansi_escape.sub('', line)
                    f.write(cleaned_line + '\n')
    except serial.SerialException as ex:
        logger.error(f"Serial port {serial_port_name} is not available or disconnected.")
        logger.error(f"Exiting due to error: {ex}")
    except KeyboardInterrupt:
        logger.error("Exiting due to 'Ctrl+C'")


def main():
    arguments = parse_arguments()

    if arguments.print_port:
        global g_flag_disable_user_interaction
        g_flag_disable_user_interaction = True

    serial_port = arguments.port
    list_of_available_ports = available_serial_ports()
    if serial_port:
        if serial_port not in list_of_available_ports:
            error(f"Serial port '{serial_port}' is not in list of available ports: {list_of_available_ports}.")
            if not ask_user_to_continue():
                sys.exit(1)
    elif serial_port := os.getenv('RUUVI_GW_SERIAL_PORT'):
        if serial_port not in list_of_available_ports:
            logger.info(f"Environment variable RUUVI_GW_SERIAL_PORT is set to {serial_port}.")
            error(f"Serial port '{serial_port}' is not in list of available ports: {list_of_available_ports}.")
            if not ask_user_to_continue():
                sys.exit(1)
    else:
        serial_port = autodetect_serial_port(list_of_available_ports)

    if arguments.print_port:
        print(serial_port)
        sys.exit(0)

    log_file_name = datetime.now().strftime(f"{arguments.log_dir}/%Y-%m-%dT%H-%M-%S_ruuvi_gw_uart.log")
    log_serial_data(serial_port, log_file_name, console_output=True)


def realpath(path):
    """
    Return the canonical path with normalized case.

    It is useful on Windows for comparison paths in case-insensitive manner.
    On Unix and Mac OS X it works as `os.path.realpath()` only.
    """
    return os.path.normcase(os.path.realpath(path))


def check_environment():
    if sys.version_info[0] < 3:
        print("ERROR: Support for Python 2 is deprecated.")
        sys.exit(1)
    elif sys.version_info[0] == 3 and sys.version_info[1] < 8:
        print("ERROR: Python 3 versions older than 3.8 are not supported.")
        sys.exit(1)

    from importlib.metadata import version as package_version, PackageNotFoundError

    list_of_requirements = requirements
    list_of_requirements_from_file = []
    requirements_path = realpath(os.path.join(os.path.dirname(__file__), "requirements.txt"))
    if os.path.isfile(requirements_path):
        with open(requirements_path) as f:
            for line in f:
                line = line.strip()
                if line == '':
                    continue
                if line.startswith('file://'):
                    line = os.path.basename(line)
                if line.startswith('-e') and '#egg=' in line:
                    # version control URLs, take the egg= part at the end only
                    line = re.search(r'#egg=(\S+)', line).group(1)
                list_of_requirements_from_file.append(line)
        diff1 = [item for item in requirements if item not in list_of_requirements_from_file]
        if len(diff1) > 0:
            print(f"WARNING: The following Python requirements are missing in the {requirements_path}: {diff1}")
            sys.exit(1)
        list_of_requirements = list_of_requirements_from_file

    not_satisfied = []
    for package in list_of_requirements:
        try:
            package_version(package)
        except PackageNotFoundError:
            not_satisfied.append(package)

    if len(not_satisfied) > 0:
        print(f'The following Python requirements are not satisfied: {not_satisfied}')
        try:
            cmd_with_args = ['pip', 'install']
            cmd_with_args.extend(not_satisfied)
            print('Install missing requirements:')
            print(' '.join(cmd_with_args))
            subprocess.check_call(cmd_with_args)
        except subprocess.CalledProcessError as e:
            error("'pip install' execution failed.")
            sys.exit(e.returncode)


if __name__ == '__main__':
    check_environment()
    import serial

    main()
