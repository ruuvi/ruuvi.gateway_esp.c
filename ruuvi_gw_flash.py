#!/usr/bin/env python3

# -----------------------------------------------------------------------------------
# File: ruuvi_gw_flash.py
# Copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
# Author: TheSomeMan
#
# Description: This script is used to handle firmware operations for the Ruuvi Gateway.
# -----------------------------------------------------------------------------------

import glob
import argparse
import subprocess
import sys
import os
import shutil
import re
from datetime import datetime
import logging
import platform
import json
import zipfile
import signal

requirements = ['requests', 'pyserial']

description = """
This script is used to handle firmware operations for the Ruuvi Gateway.
It can download and write specific firmware version binaries from GitHub to the device.
It also has the option to erase flash before writing the firmware,
or flash only specific binaries (ruuvi_gateway_esp.bin and ota_data_initial.bin) after compiling the project.

Serial port can be specified, if not, the script will automatically detect the only available port,
if none or multiple ports are available, it will prompt the user to specify.
Also it is possible to specify the serial port in the environment variable RUUVI_GW_SERIAL_PORT:
`export RUUVI_GW_SERIAL_PORT=/dev/ttyUSB1`

Possible operations:
1) [download (if needed) and flash specific version]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] v1.15.0
2.1) [download and flash GitHub artifact]: python3 ruuvi_gw_flash.py https://github.com/ruuvi/ruuvi.gateway_esp.c/actions/runs/8187982688
2.2) [download and flash GitHub artifact]: python3 ruuvi_gw_flash.py 8187982688
3) [only erase flash]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] --erase_flash -
4) [download, erase and flash specific version]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] --erase_flash v1.15.0
5) [Build and flash]: python3 ruuvi_gw_flash.py build
6) [Compile and flash only ruuvi_gateway_esp.bin and ota_data_initial.bin]: python3 ruuvi_gw_flash.py --compile_and_flash build
7) [Compile and flash only ruuvi_gateway_esp.bin and ota_data_initial.bin]: python3 ruuvi_gw_flash.py --compile_only build
8) [Reset]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] --reset -
9) [Reset and save UART logs]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] - --reset --log_uart
"""
parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

RELEASES_DIR = '.releases'

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


def check_if_release_exist_on_github(version):
    response = requests.get("https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases")
    if response.status_code != 200:
        error("Could not fetch releases from GitHub.")
        sys.exit(1)
    releases = response.json()
    for release in releases:
        if release['tag_name'] == version:
            return True, releases
    return False, releases


def parse_url(url):
    pattern = r"^https://github\.com/ruuvi/ruuvi\.gateway_esp\.c/actions/runs/(\d+)/artifacts/\d+$"
    match = re.match(pattern, url)
    if match is not None:
        return match.group(1)

    pattern = r"^https://github\.com/ruuvi/ruuvi\.gateway_esp\.c/actions/runs/(\d+)$"
    match = re.match(pattern, url)
    if match is not None:
        return match.group(1)

    return None


def copy_file_to_parent_dir(folder, filename):
    """Copy file from the specified folder to its parent directory."""
    file_path = str(os.path.join(folder, filename))
    parent_folder = os.path.dirname(folder)
    destination_path = str(os.path.join(parent_folder, filename))

    shutil.copy(file_path, destination_path)
    logger.info(f"Copied {filename} to parent directory.")


def download_binary_by_github_run_id(github_run_id, fw_ver_dir):
    if shutil.which('gh') is None:
        logger.error(
            "GitHub CLI is not installed. Please install it from https://cli.github.com/")
        sys.exit(1)
    if not os.path.isdir(fw_ver_dir):
        logger.info(f'Creating directory: {fw_ver_dir}')
        os.mkdir(fw_ver_dir)

    logger.info(f"Downloading the list of artifacts for the action {github_run_id}...")
    cmd_with_args = ['gh', 'api', f'repos/ruuvi/ruuvi.gateway_esp.c/actions/runs/{github_run_id}/artifacts']
    logger.info(' '.join(cmd_with_args))
    try:
        process_output = subprocess.run(cmd_with_args, capture_output=True, check=True, text=True)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to download the GitHub action {github_run_id} with GitHub CLI: {e}")
        sys.exit(1)
    try:
        json_list_of_artifacts = json.loads(process_output.stdout)
    except json.JSONDecodeError as e:
        logger.error(f"Failed to parse the list of artifacts for GitHub action {github_run_id}: {e}")
        logger.info(f"Output: {process_output.stdout}")
        sys.exit(1)

    artifact_name = 'ruuvi_gateway_fw'
    artifact_id = None
    try:
        for artifact in json_list_of_artifacts["artifacts"]:
            if artifact["name"] == 'ruuvi_gateway_fw':
                artifact_id = artifact["id"]
    except KeyError as e:
        logger.error(f"Failed to find the artifact ID for GitHub action {github_run_id}: {e}")
        logger.info(f"JSON: {process_output.stdout}")
        sys.exit(1)

    logger.info(f"Downloading the artifact {artifact_id} with GitHub CLI to {fw_ver_dir}...")
    cmd_with_args = ['gh', 'api', f'repos/ruuvi/ruuvi.gateway_esp.c/actions/artifacts/{artifact_id}/zip']
    logger.info(' '.join(cmd_with_args))
    try:
        artifact_zip = subprocess.run(cmd_with_args, capture_output=True, check=True, text=False)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to download the artifact {artifact_id} with GitHub CLI: {e}")
        sys.exit(1)
    artifact_zip_file_path = f'{fw_ver_dir}/{artifact_name}.zip'
    with open(artifact_zip_file_path, 'wb') as file:
        file.write(artifact_zip.stdout)
    with zipfile.ZipFile(artifact_zip_file_path, 'r') as zip_ref:
        zip_ref.extractall(fw_ver_dir)
    os.remove(artifact_zip_file_path)
    copy_file_to_parent_dir(f'{fw_ver_dir}/binaries_v1.9.2', 'bootloader.bin')
    shutil.rmtree(f'{fw_ver_dir}/binaries_v1.9.2')
    copy_file_to_parent_dir(f'{fw_ver_dir}/partition_table', 'partition-table.bin')
    shutil.rmtree(f'{fw_ver_dir}/partition_table')


def download_binary(version, fw_ver_dir, filename):
    url = f"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/{version}/{filename}"
    response = requests.get(url, stream=True)

    if response.status_code == 200:
        logger.info(f'Downloading: {fw_ver_dir}/{filename}')
        with open(f"{fw_ver_dir}/{filename}", 'wb') as f:
            f.write(response.content)
    else:
        raise Exception(f"Could not download the binary {filename}.")


def download_binaries_if_needed(fw_ver):
    if not os.path.isdir(RELEASES_DIR):
        logger.info(f'Creating directory: {RELEASES_DIR} ({os.path.realpath(RELEASES_DIR)})')
        os.mkdir(RELEASES_DIR)

    github_run_id = None
    if fw_ver.startswith('https://'):
        github_run_id = parse_url(fw_ver)
        if github_run_id is None:
            error(f"Invalid URL: {fw_ver}")
            sys.exit(1)
    elif re.fullmatch(r'^\d+$', fw_ver):
        github_run_id = fw_ver
    if github_run_id:
        fw_ver_dir = os.path.realpath(f'{RELEASES_DIR}/{github_run_id}')
        flag_need_to_download = False
        if not os.path.isdir(fw_ver_dir):
            flag_need_to_download = True
        else:
            for file in ['bootloader.bin', 'partition-table.bin', 'ota_data_initial.bin', 'ruuvi_gateway_esp.bin',
                         'fatfs_gwui.bin', 'fatfs_nrf52.bin']:
                if not os.path.isfile(f'{fw_ver_dir}/{file}'):
                    flag_need_to_download = True
                    break
        if flag_need_to_download:
            try:
                download_binary_by_github_run_id(github_run_id, fw_ver_dir)
            except Exception as e:
                logger.error(str(e))
                shutil.rmtree(fw_ver_dir)
                sys.exit(1)
        else:
            logger.info(f'Directory already exists: {fw_ver_dir}')
        return github_run_id

    fw_ver_dir = os.path.realpath(f'{RELEASES_DIR}/{fw_ver}')
    directory_exists = os.path.isdir(fw_ver_dir)

    if not directory_exists:
        logger.info(f'Checking if the release {fw_ver} exists on GitHub...')
        release_exists, releases = check_if_release_exist_on_github(fw_ver)

        if not directory_exists and not release_exists:
            error(f'No such firmware version "{fw_ver}" exists on disk or GitHub.')
            list_of_releases = ', '.join(release['tag_name'] for release in releases)
            logger.info(f'Available releases: [{list_of_releases}]')
            sys.exit(1)

        logger.info(f'Creating directory: {fw_ver_dir}')
        os.makedirs(fw_ver_dir)

        firmware_files = [
            'bootloader.bin',
            'partition-table.bin',
            'ota_data_initial.bin',
            'ruuvi_gateway_esp.bin',
            'fatfs_gwui.bin',
            'fatfs_nrf52.bin'
        ]

        try:
            for file in firmware_files:
                download_binary(fw_ver, fw_ver_dir, file)
        except Exception as e:
            logger.error(str(e))
            shutil.rmtree(fw_ver_dir)
            sys.exit(1)
    else:
        logger.info(f'Directory already exists: {fw_ver_dir}')
    return fw_ver


def parse_arguments():
    parser.add_argument('--port',
                        type=str,
                        default=None,
                        help='Serial Port')

    parser.add_argument('fw_ver',
                        type=str,
                        help='Firmware version to write, URL of GitHub action or GitHub action run_id. '
                             'Use "-" to skip flashing firmware binaries.')

    parser.add_argument('--erase_flash',
                        action='store_true',
                        help='Should the flash be erased before writing firmware. '
                             'Use together with "-" for fw_ver to erase flash only.')

    parser.add_argument('--reset',
                        action='store_true',
                        help='Reset the gateway. Use together with "-" for fw_ver.')

    parser.add_argument('--compile_and_flash',
                        action='store_true',
                        help='Compile the project and flash only ruuvi_gateway_esp.bin and ota_data_initial.bin '
                             '(use with "build" as fw_ver.')

    parser.add_argument('--compile_only',
                        action='store_true',
                        help='Only compile the project '
                             '(use with "build" as fw_ver.')

    parser.add_argument('--download_only',
                        action='store_true',
                        help='Download the firmware binaries only, do not flash them.')

    parser.add_argument('--log_uart',
                        action='store_true',
                        help='Log UART output to a file.')

    parser.add_argument('--log_to_console',
                        action='store_true',
                        help='Log UART output to console.')

    parser.add_argument('--log',
                        action='store_true',
                        help='Save log of the script execution to file.')

    parser.add_argument('--log_dir',
                        type=str,
                        default='.',
                        help='Directory to save log files.')

    parser.add_argument('--print_port',
                        action='store_true',
                        help='Print the only available serial port and exit.')

    arguments = parser.parse_args()

    if arguments.log_to_console:
        arguments.log_uart = True
    if (arguments.fw_ver == "-" and not arguments.erase_flash and not arguments.reset and not arguments.log_uart and
            not arguments.print_port):
        error("Nothing to do: "
              "'-' is passed as 'fw_ver' but '--erase_flash', '--reset', '--log_uart' or '--print_port' is not set")
        sys.exit(1)
    if (arguments.fw_ver == "-" or arguments.fw_ver == "build") and arguments.download_only:
        error("Argument '--download_only' requires 'fw_ver' to be set.")
        sys.exit(1)
    if arguments.erase_flash and arguments.download_only:
        error("Arguments '--download_only' and '--erase_flash' are mutually exclusive.")
        sys.exit(1)
    if arguments.erase_flash and arguments.compile_and_flash:
        error("'--erase_flash' and '--compile_and_flash' cannot be used together.")
        sys.exit(1)
    if arguments.compile_only and arguments.compile_and_flash:
        error("'--compile_only' and '--compile_and_flash' cannot be used together.")
        sys.exit(1)
    if arguments.erase_flash and arguments.log_uart:
        error("'--erase_flash' and '--log_uart' cannot be used together.")
        sys.exit(1)
    if arguments.compile_and_flash and arguments.fw_ver != "build":
        error("'--compile_and_flash' must be used with 'build' as fw_ver.")
        sys.exit(1)
    if arguments.compile_only and arguments.fw_ver != "build":
        error("'--compile_only' must be used with 'build' as fw_ver.")
        sys.exit(1)

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


def run_process_with_logging(cmd_with_args):
    logger.info(' '.join(cmd_with_args))
    try:
        with subprocess.Popen(cmd_with_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=1,
                              universal_newlines=True) as proc:
            for line in proc.stdout:
                logger.info(line.strip())
        if proc.returncode != 0:
            logger.error(f"{cmd_with_args[0]} execution failed with code {proc.returncode}")
            sys.exit(proc.returncode)
    except subprocess.CalledProcessError as e:
        logger.error(f"{cmd_with_args[0]} execution failed: {e}")
        sys.exit(e.returncode)


def main():
    arguments = parse_arguments()

    if arguments.log:
        f_handler = logging.FileHandler(datetime.now().strftime(f"{arguments.log_dir}/%Y-%m-%dT%H-%M-%S_ruuvi_gw_flash.log"))
        f_handler.setLevel(logging.INFO)
        f_format = logging.Formatter('[%(asctime)s.%(msecs)03d %(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
        f_handler.setFormatter(f_format)
        logger.addHandler(f_handler)

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
        if not arguments.download_only:
            serial_port = autodetect_serial_port(list_of_available_ports)

    if arguments.print_port:
        print(serial_port)
        sys.exit(0)

    if arguments.fw_ver != "-" and arguments.fw_ver != "build":
        arguments.fw_ver = download_binaries_if_needed(arguments.fw_ver)

    if shutil.which('esptool.py') is not None:
        esptool = 'esptool.py'
    elif shutil.which('esptool') is not None:
        esptool = 'esptool'
    else:
        error("esptool.py is not installed.")
        if not ask_user_to_continue():
            sys.exit(1)
        esptool = 'esptool.py'

    esptool_base_cmd_with_args = [esptool, '-p', serial_port, '-b', '460800', '--before', 'default_reset',
                                  '--after', 'hard_reset', '--chip', 'esp32']

    if arguments.erase_flash:
        esptool_cmd_with_args = esptool_base_cmd_with_args[:]
        esptool_cmd_with_args.append('erase_flash')
        run_process_with_logging(esptool_cmd_with_args)

    if arguments.compile_only:
        if not os.path.isdir("build"):
            logger.error('fw_ver must be "build"')
            sys.exit(1)
        else:
            logger.info('cd build')
            os.chdir("build")
            run_process_with_logging(['ninja', 'ruuvi_gateway_esp.elf'])
            run_process_with_logging(['ninja', '.bin_timestamp'])
            logger.info('cd ..')
            os.chdir("..")
            sys.exit(0)
    if arguments.compile_and_flash:
        if not os.path.isdir("build"):
            arguments.compile_and_flash = False
        else:
            logger.info('cd build')
            os.chdir("build")
            run_process_with_logging(['ninja', 'ruuvi_gateway_esp.elf'])
            run_process_with_logging(['ninja', '.bin_timestamp'])
            logger.info('cd ..')
            os.chdir("..")
    if not arguments.compile_and_flash and arguments.fw_ver == "build":
        if shutil.which('idf.py') is None:
            error("idf.py not found.")
            if not ask_user_to_continue():
                sys.exit(1)
        run_process_with_logging(['idf.py', 'build'])

    if not arguments.download_only and arguments.fw_ver != "-":
        esptool_cmd_with_args = esptool_base_cmd_with_args[:]
        esptool_cmd_with_args.append('write_flash')
        esptool_cmd_with_args += ['--flash_mode', 'dio', '--flash_size', 'detect', '--flash_freq', '40m']
        if arguments.compile_and_flash:
            esptool_cmd_with_args += [
                '0xd000', f'{arguments.fw_ver}/ota_data_initial.bin',
                '0x100000', f'{arguments.fw_ver}/ruuvi_gateway_esp.bin']
        else:
            if arguments.fw_ver == 'build':
                esptool_cmd_with_args += [
                    '0x1000', f'{arguments.fw_ver}/binaries_v1.9.2/bootloader.bin',
                    '0x8000', f'{arguments.fw_ver}/partition_table/partition-table.bin',
                    '0xd000', f'{arguments.fw_ver}/ota_data_initial.bin',
                    '0x100000', f'{arguments.fw_ver}/ruuvi_gateway_esp.bin',
                    '0x500000', f'{arguments.fw_ver}/fatfs_gwui.bin',
                    '0x5C0000', f'{arguments.fw_ver}/fatfs_nrf52.bin']
            else:
                esptool_cmd_with_args += [
                    '0x1000', f'{RELEASES_DIR}/{arguments.fw_ver}/bootloader.bin',
                    '0x8000', f'{RELEASES_DIR}/{arguments.fw_ver}/partition-table.bin',
                    '0xd000', f'{RELEASES_DIR}/{arguments.fw_ver}/ota_data_initial.bin',
                    '0x100000', f'{RELEASES_DIR}/{arguments.fw_ver}/ruuvi_gateway_esp.bin',
                    '0x500000', f'{RELEASES_DIR}/{arguments.fw_ver}/fatfs_gwui.bin',
                    '0x5C0000', f'{RELEASES_DIR}/{arguments.fw_ver}/fatfs_nrf52.bin']
        run_process_with_logging(esptool_cmd_with_args)

    if arguments.reset:
        esptool_cmd_with_args = esptool_base_cmd_with_args[:]
        esptool_cmd_with_args.append('run')
        run_process_with_logging(esptool_cmd_with_args)

    if arguments.log_uart:
        log_file_name = datetime.now().strftime(f"{arguments.log_dir}/%Y-%m-%dT%H-%M-%S_ruuvi_gw_uart.log")
        log_serial_data(serial_port, log_file_name, console_output=arguments.log_to_console)


def realpath(path):
    """
    Return the cannonical path with normalized case.

    It is useful on Windows to comparision paths in case-insensitive manner.
    On Unix and Mac OS X it works as `os.path.realpath()` only.
    """
    return os.path.normcase(os.path.realpath(path))


def check_environment():
    if sys.version_info[0] < 3:
        print("WARNING: Support for Python 2 is deprecated and will be removed in future versions.")
    elif sys.version_info[0] == 3 and sys.version_info[1] < 8:
        print("WARNING: Python 3 versions older than 3.8 are not supported.")

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
        if len(list_of_requirements_from_file) > 0:
            print(f'Install them using the command: pip install -r {requirements_path}')
        else:
            print(f'Install them using the command: pip install {" ".join(not_satisfied)}')
        sys.exit(1)


if __name__ == '__main__':
    check_environment()
    import requests

    main()
