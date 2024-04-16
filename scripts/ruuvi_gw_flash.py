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
import requests
import shutil
import zipfile
import re

description = """
This script is used to handle firmware operations for the Ruuvi Gateway.
It can download and write specific firmware version binaries from GitHub to the device.
It also has the option to erase flash before writing the firmware,
or flash only specific binaries (ruuvi_gateway_esp.bin and ota_data_initial.bin) after compiling the project.

Serial port can be specified, if not, the script will automatically detect the only available port,
if none or multiple ports are available, it will prompt the user to specify.

Possible operations:
1) [download (if needed) and flash specific version]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] v1.15.0
2) [download and flash GitHub artifact]: python3 ruuvi_gw_flash.py https://github.com/ruuvi/ruuvi.gateway_esp.c/actions/runs/8187982688/artifacts/1305715066
3) [only erase flash]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] --erase_flash -
4) [download, erase and flash specific version]: python3 ruuvi_gw_flash.py [--port /dev/ttyUSB0] --erase_flash v1.15.0
5) [Build and flash]: python3 ruuvi_gw_flash.py build
6) [Compile and flash only ruuvi_gateway_esp.bin and ota_data_initial.bin]: python3 ruuvi_gw_flash.py --compile_and_flash build
"""
parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

RELEASES_DIR = '.releases'


# Function to print error messages in red
def error(message):
    color_red = '\033[0;31m'
    color_no = '\033[0m'  # No Color
    print(f"{color_red}Error: {message}{color_no}")


def print_usage():
    parser.print_help()


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
    pattern = r"^https://github\.com/ruuvi/ruuvi\.gateway_esp\.c/actions/runs/\d+/artifacts/(\d+)$"
    match = re.match(pattern, url)
    if match is not None:
        return match.group(1)
    else:
        return None


def copy_file_to_parent_dir(folder, filename):
    """Copy file from the specified folder to its parent directory."""
    file_path = str(os.path.join(folder, filename))
    parent_folder = os.path.dirname(folder)
    destination_path = str(os.path.join(parent_folder, filename))

    shutil.copy(file_path, destination_path)
    print(f"Copied {filename} to parent directory.")


def download_binary_from_url(url, fw_ver_dir):
    token = os.getenv('GITHUB_TOKEN')
    headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": "2022-11-28"
    }

    response = requests.get(url, headers=headers, stream=True)

    filename = 'ruuvi_gateway_fw.zip'
    if response.status_code == 200:
        print(f'\nDownloading: {url}...', end="", flush=True)
        with open(f"{fw_ver_dir}/{filename}", 'wb') as fd:
            for chunk in response.iter_content(chunk_size=10240):
                fd.write(chunk)
        print('\nExtracting files...', end='', flush=True)
        with zipfile.ZipFile(f"{fw_ver_dir}/{filename}", 'r') as zip_ref:
            zip_ref.extractall(fw_ver_dir)
            print('')
            copy_file_to_parent_dir(f'{fw_ver_dir}/binaries_v1.9.2', 'bootloader.bin')
            shutil.rmtree(f'{fw_ver_dir}/binaries_v1.9.2')
            copy_file_to_parent_dir(f'{fw_ver_dir}/partition_table', 'partition-table.bin')
            shutil.rmtree(f'{fw_ver_dir}/partition_table')
    else:
        raise Exception(f'Could not download the binary {filename}, '
                        f'HTTP error {response.status_code}: {response.text}.')


def download_binary(version, fw_ver_dir, filename):
    url = f"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/{version}/{filename}"
    response = requests.get(url, stream=True)

    if response.status_code == 200:
        print(f'\nDownloading: {fw_ver_dir}/{filename}...', end="", flush=True)  # Print file name
        with open(f"{fw_ver_dir}/{filename}", 'wb') as f:
            f.write(response.content)
    else:
        raise Exception(f"Could not download the binary {filename}.")


def download_binaries_if_needed(fw_ver):
    if not os.path.isdir(RELEASES_DIR):
        os.mkdir(RELEASES_DIR)

    if fw_ver.startswith('https://'):
        artifact_id = parse_url(fw_ver)
        if artifact_id is None:
            error(f"Invalid URL: {fw_ver}")
            sys.exit(1)
        url = f'https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/actions/artifacts/{artifact_id}/zip'
        fw_ver_dir = f'{RELEASES_DIR}/{artifact_id}'
        if not os.path.isdir(fw_ver_dir):
            os.mkdir(fw_ver_dir)
            try:
                download_binary_from_url(url, fw_ver_dir)
            except Exception as e:
                error(str(e))
                shutil.rmtree(fw_ver_dir)
                sys.exit(1)
            pass
        return artifact_id

    fw_ver_dir = f'{RELEASES_DIR}/{fw_ver}'
    directory_exists = os.path.isdir(fw_ver_dir)

    if not directory_exists:
        print(f'Checking if the release {fw_ver} exists on GitHub...', end="", flush=True)
        release_exists, releases = check_if_release_exist_on_github(fw_ver)
        print('\n', end='', flush=True)

        if not directory_exists and not release_exists:
            error(f'No such firmware version "{fw_ver}" exists on disk or GitHub.')
            print('Available releases: [', end='')
            print(', '.join(release['tag_name'] for release in releases), end='')
            print(']')
            sys.exit(1)

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
            print('\n', end="", flush=True)
        except Exception as e:
            error(str(e))
            shutil.rmtree(fw_ver_dir)
            sys.exit(1)
    return fw_ver


def parse_arguments():
    parser.add_argument('--port',
                        type=str,
                        default=None,
                        help='Serial Port')

    parser.add_argument('fw_ver',
                        type=str,
                        help='Firmware version to write or URL of GitHub action artifact. '
                             'Use "-" to skip flashing firmware binaries.')

    parser.add_argument('--erase_flash',
                        action='store_true',
                        help='Should the flash be erased before writing firmware. '
                             'Use together with "-" for fw_ver to erase flash only.')

    parser.add_argument('--compile_and_flash',
                        action='store_true',
                        help='Compile the project and flash only ruuvi_gateway_esp.bin and ota_data_initial.bin '
                             '(use with "build" as fw_ver.')

    parser.add_argument('--download_only',
                        action='store_true',
                        help='Download the firmware binaries only, do not flash them.')

    arguments = parser.parse_args()

    if arguments.fw_ver == "-" and not arguments.erase_flash:
        error("Nothing to do: '-' is passed as 'fw_ver' but '--erase_flash' is not set")
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
    if arguments.compile_and_flash and arguments.fw_ver != "build":
        error("'--compile_and_flash' must be used with 'build' as fw_ver.")
        sys.exit(1)

    return arguments


def autodetect_serial_port():
    # Find ports
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/cu.wchusbserial*') + glob.glob('/dev/tty.usbserial-*')

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


def main():
    arguments = parse_arguments()

    serial_port = arguments.port
    if not serial_port and not arguments.download_only:
        serial_port = autodetect_serial_port()

    if arguments.fw_ver != "-" and arguments.fw_ver != "build":
        arguments.fw_ver = download_binaries_if_needed(arguments.fw_ver)

    esptool_base_cmd_with_args = ['esptool.py', '-p', serial_port, '-b', '460800', '--before', 'default_reset',
                                  '--after', 'hard_reset', '--chip', 'esp32']

    if arguments.erase_flash:
        esptool_cmd_with_args = esptool_base_cmd_with_args[:]
        esptool_cmd_with_args.append('erase_flash')
        try:
            print(' '.join(esptool_cmd_with_args))
            subprocess.check_call(esptool_cmd_with_args)
        except subprocess.CalledProcessError as e:
            error("esptool.py execution failed.")
            sys.exit(e.returncode)

    if arguments.compile_and_flash:
        if not os.path.isdir("build"):
            arguments.compile_and_flash = False
        else:
            print('cd build')
            os.chdir("build")
            try:
                print(' '.join(['ninja', 'ruuvi_gateway_esp.elf']))
                subprocess.check_call(['ninja', 'ruuvi_gateway_esp.elf'])
                print(' '.join(['ninja', '.bin_timestamp']))
                subprocess.check_call(['ninja', '.bin_timestamp'])
            except subprocess.CalledProcessError as e:
                error(f"'ninja' command execution failed with error code: {e.returncode}")
                sys.exit(e.returncode)
            print('cd ..')
            os.chdir("..")
    if not arguments.compile_and_flash and arguments.fw_ver == "build":
        try:
            print(' '.join(['idf.py', 'build']))
            subprocess.check_call(['idf.py', 'build'])
        except subprocess.CalledProcessError as e:
            error(f"'idf.py build' command execution failed with error code: {e.returncode}")
            sys.exit(e.returncode)

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
        try:
            print(' '.join(esptool_cmd_with_args))
            subprocess.check_call(esptool_cmd_with_args)
        except subprocess.CalledProcessError as e:
            error("esptool.py execution failed.")
            sys.exit(e.returncode)


if __name__ == '__main__':
    main()
