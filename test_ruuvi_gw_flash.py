import argparse
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest.mock import call, patch

import ruuvi_gw_flash


class FlashBothSlotsTests(unittest.TestCase):
    FIRST_SLOT_FILES = {
        '0x1000': 'bootloader.bin',
        '0x8000': 'partition-table.bin',
        '0xd000': 'ota_data_initial.bin',
        '0x100000': 'ruuvi_gateway_esp.bin',
        '0x500000': 'fatfs_gwui.bin',
        '0x5C0000': 'fatfs_nrf52.bin',
    }
    SECOND_SLOT_FILES = {
        '0x600000': 'ruuvi_gateway_esp.bin',
        '0xA00000': 'fatfs_gwui.bin',
        '0xAC0000': 'fatfs_nrf52.bin',
    }
    APP_ONLY_FILES = {
        '0xd000': 'ota_data_initial.bin',
        '0x100000': 'ruuvi_gateway_esp.bin',
    }

    def setUp(self):
        original_dir = os.getcwd()
        temp_dir = tempfile.TemporaryDirectory()
        self.work_dir = temp_dir.name
        self.addCleanup(temp_dir.cleanup)
        self.addCleanup(os.chdir, original_dir)
        os.chdir(self.work_dir)
        environment = patch.dict(os.environ)
        environment.start()
        self.addCleanup(environment.stop)
        os.environ.pop('RUUVI_GW_SERIAL_PORT', None)

        self.run_process = self.patch('run_process_with_logging')
        self.download = self.patch('download_binaries_if_needed', side_effect=lambda version: version)
        self.available_ports = self.patch('available_serial_ports', return_value=['/dev/ttyUSB0'])
        self.log_serial = self.patch('log_serial_data')
        self.error = self.patch('error')
        self.logger = self.patch('logger')
        self.patch('g_flag_disable_user_interaction', False)
        self.ask = self.patch('ask_user_to_continue', return_value=False)
        which = patch.object(ruuvi_gw_flash.shutil, 'which', side_effect=lambda tool: f'/usr/bin/{tool}')
        self.which = which.start()
        self.addCleanup(which.stop)

    def patch(self, name, *args, **kwargs):
        patcher = patch.object(ruuvi_gw_flash, name, *args, **kwargs)
        result = patcher.start()
        self.addCleanup(patcher.stop)
        return result

    def create_images(self, directory, names=None):
        if names is None:
            names = list(ruuvi_gw_flash.RELEASE_ZIP_FILES)
            if directory == 'build':
                names += ['binaries_v1.9.2/bootloader.bin', 'partition_table/partition-table.bin']
        for name in names:
            path = os.path.join(directory, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as image:
                image.write(b'image')

    def run_flash(self, *args, port='/dev/ttyUSB0'):
        self.run_process.reset_mock()
        self.error.reset_mock()
        port_args = ['--port', port] if port is not None else []
        with patch.object(sys, 'argv', ['ruuvi_gw_flash.py', *port_args, *args]), \
                patch.object(ruuvi_gw_flash, 'parser', argparse.ArgumentParser()):
            ruuvi_gw_flash.main()
        return [item.args[0] for item in self.run_process.call_args_list]

    def parse_arguments(self, *args):
        with patch.object(sys, 'argv', ['ruuvi_gw_flash.py', *args]), \
                patch.object(ruuvi_gw_flash, 'parser', argparse.ArgumentParser()):
            return ruuvi_gw_flash.parse_arguments()

    def flash_files(self, commands, port='/dev/ttyUSB0', esptool='esptool.py'):
        writes = [command for command in commands if 'write_flash' in command]
        self.assertEqual(len(writes), 1, commands)
        prefix = [
            esptool, '-p', port, '-b', '460800', '--before', 'default_reset',
            '--after', 'hard_reset', '--chip', 'esp32', 'write_flash',
            '--flash_mode', 'dio', '--flash_size', 'detect', '--flash_freq', '40m',
        ]
        self.assertEqual(writes[0][:len(prefix)], prefix)
        pairs = writes[0][len(prefix):]
        self.assertEqual(len(pairs) % 2, 0)
        addresses = pairs[::2]
        self.assertEqual(len(addresses), len(set(addresses)), 'Duplicate flash addresses')
        self.assertEqual(addresses, sorted(addresses, key=lambda value: int(value, 16)))
        return dict(zip(addresses, pairs[1::2]))

    def expected_files(self, directory, both_slots=False, config=False, app_only=False):
        files = dict(self.APP_ONLY_FILES if app_only else self.FIRST_SLOT_FILES)
        if directory == 'build' and not app_only:
            files['0x1000'] = 'binaries_v1.9.2/bootloader.bin'
            files['0x8000'] = 'partition_table/partition-table.bin'
        if both_slots:
            files.update(self.SECOND_SLOT_FILES)
        if config:
            files['0xB00000'] = 'gw_cfg_def.bin'
        return {address: f'{directory}/{name}' for address, name in files.items()}

    def test_release_flash_matrix(self):
        for version in ['v1.15.0', 'v1.17.2-dev', 'v1.17.2-prod']:
            directory = f'.releases/{version}'
            self.create_images(directory)
            for both_slots in [False, True]:
                for config in [False, True]:
                    with self.subTest(version=version, both_slots=both_slots, config=config):
                        args = [version]
                        if both_slots:
                            args.append('--flash_both_slots')
                        if config:
                            args.append('--flash_gw_cfg_def')

                        commands = self.run_flash(*args)

                        self.assertEqual(len(commands), 1)
                        self.assertEqual(self.flash_files(commands),
                                         self.expected_files(directory, both_slots, config))
                        self.download.assert_called_with(version)

    def test_artifacts_use_downloaded_cache_directory(self):
        run_id = '8187982688'
        directory = f'.releases/{run_id}'
        self.create_images(directory)
        self.download.side_effect = None
        self.download.return_value = run_id
        for source in [
                run_id,
                f'https://github.com/ruuvi/ruuvi.gateway_esp.c/actions/runs/{run_id}',
                f'https://github.com/ruuvi/ruuvi.gateway_esp.c/actions/runs/{run_id}/artifacts/123']:
            for both_slots in [False, True]:
                for config in [False, True]:
                    with self.subTest(source=source, both_slots=both_slots, config=config):
                        args = [source]
                        if both_slots:
                            args.append('--flash_both_slots')
                        if config:
                            args.append('--flash_gw_cfg_def')

                        commands = self.run_flash(*args)

                        self.assertEqual(self.flash_files(commands),
                                         self.expected_files(directory, both_slots, config))
                        self.download.assert_called_with(source)

    def test_full_build_flash_matrix(self):
        self.create_images('build')
        for both_slots in [False, True]:
            for config in [False, True]:
                with self.subTest(both_slots=both_slots, config=config):
                    args = ['build']
                    if both_slots:
                        args.append('--flash_both_slots')
                    if config:
                        args.append('--flash_gw_cfg_def')

                    commands = self.run_flash(*args)

                    self.assertEqual(commands[:-1], [['idf.py', 'build']])
                    self.assertEqual(self.flash_files(commands),
                                     self.expected_files('build', both_slots, config))
        self.download.assert_not_called()

    def test_fresh_full_build_creates_images_before_flashing(self):
        def build_images(command):
            if command == ['idf.py', 'build']:
                self.create_images('build')

        self.run_process.side_effect = build_images

        commands = self.run_flash('build', '--flash_both_slots', '--flash_gw_cfg_def')

        self.assertEqual(commands[:-1], [['idf.py', 'build']])
        self.assertEqual(self.flash_files(commands), self.expected_files('build', True, True))

    def test_compile_and_flash_writes_only_app_and_ota_data(self):
        for existing_build in [False, True]:
            with self.subTest(existing_build=existing_build):
                if existing_build:
                    self.create_images('build', self.APP_ONLY_FILES.values())

                commands = self.run_flash('--compile_and_flash', 'build')

                self.assertEqual(self.flash_files(commands), self.expected_files('build', app_only=True))
                expected_build = ([['ninja', 'ruuvi_gateway_esp.elf'], ['ninja', '.bin_timestamp']]
                                  if existing_build else [['idf.py', 'build']])
                self.assertEqual(commands[:-1], expected_build)
        self.download.assert_not_called()

    def test_compile_and_flash_can_include_default_config(self):
        self.create_images('build', ['ota_data_initial.bin', 'ruuvi_gateway_esp.bin', 'gw_cfg_def.bin'])

        commands = self.run_flash('--compile_and_flash', '--flash_gw_cfg_def', 'build')

        self.assertEqual(self.flash_files(commands), self.expected_files('build', config=True, app_only=True))

    def test_fresh_compile_and_flash_can_include_default_config(self):
        def build_images(command):
            if command == ['idf.py', 'build']:
                self.create_images('build')

        self.run_process.side_effect = build_images

        commands = self.run_flash('--compile_and_flash', '--flash_gw_cfg_def', 'build')

        self.assertEqual(commands[:-1], [['idf.py', 'build']])
        self.assertEqual(self.flash_files(commands), self.expected_files('build', config=True, app_only=True))

    def test_default_config_image_is_optional_without_flag(self):
        for source in ['build', 'v1.15.0']:
            directory = source if source == 'build' else f'.releases/{source}'
            self.create_images(directory)
            os.remove(f'{directory}/gw_cfg_def.bin')
            with self.subTest(source=source):
                commands = self.run_flash(source, '--flash_both_slots')
                self.assertEqual(self.flash_files(commands), self.expected_files(directory, True))

    def test_missing_slot_images_prevent_write_flash(self):
        for source in ['build', 'v1.17.2-prod']:
            directory = source if source == 'build' else f'.releases/{source}'
            for filename in ['ruuvi_gateway_esp.bin', 'fatfs_gwui.bin', 'fatfs_nrf52.bin']:
                with self.subTest(source=source, filename=filename):
                    self.create_images(directory)
                    missing_file = f'{directory}/{filename}'
                    os.remove(missing_file)

                    with self.assertRaises(SystemExit) as result:
                        self.run_flash(source, '--flash_both_slots')

                    self.assertEqual(result.exception.code, 1)
                    self.error.assert_called_with(f'Required file does not exist: {missing_file}')
                    self.assertFalse(any('write_flash' in item.args[0]
                                         for item in self.run_process.call_args_list))

    def test_missing_requested_default_config_prevents_write_flash(self):
        for source, options in [
                ('build', []), ('build', ['--flash_both_slots']), ('build', ['--compile_and_flash']),
                ('v1.17.2-prod', []), ('v1.17.2-prod', ['--flash_both_slots'])]:
            with self.subTest(source=source, options=options):
                directory = source if source == 'build' else f'.releases/{source}'
                self.create_images(directory)
                os.remove(f'{directory}/gw_cfg_def.bin')

                with self.assertRaises(SystemExit) as result:
                    self.run_flash(source, '--flash_gw_cfg_def', *options)

                self.assertEqual(result.exception.code, 1)
                self.error.assert_called_with(f'Required file does not exist: {directory}/gw_cfg_def.bin')
                self.assertFalse(any('write_flash' in item.args[0]
                                     for item in self.run_process.call_args_list))

    def test_erase_precedes_firmware_write(self):
        for source in ['build', 'v1.17.2-prod']:
            directory = source if source == 'build' else f'.releases/{source}'
            self.create_images(directory)
            for both_slots in [False, True]:
                with self.subTest(source=source, both_slots=both_slots):
                    args = [source, '--erase_flash']
                    if both_slots:
                        args.append('--flash_both_slots')

                    commands = self.run_flash(*args)

                    self.assertEqual(commands[0][-1], 'erase_flash')
                    self.assertIn('write_flash', commands[-1])
                    self.assertEqual(self.flash_files(commands), self.expected_files(directory, both_slots))

    def test_reset_and_uart_logging_follow_flashing(self):
        self.create_images('.releases/v1.17.2-prod')
        events = []
        self.run_process.side_effect = lambda command: events.append(command)
        self.log_serial.side_effect = lambda *args, **kwargs: events.append('log_uart')

        commands = self.run_flash('v1.17.2-prod', '--flash_both_slots', '--reset',
                                  '--log_to_console', '--log_dir', 'logs')

        self.assertIn('write_flash', events[0])
        self.assertEqual(events[1][-1], 'run')
        self.assertEqual(events[2], 'log_uart')
        self.assertEqual(len(events), 3)
        self.assertEqual(self.flash_files(commands), self.expected_files('.releases/v1.17.2-prod', True))
        self.log_serial.assert_called_once()
        args, kwargs = self.log_serial.call_args
        self.assertEqual(args[0], '/dev/ttyUSB0')
        self.assertTrue(args[1].startswith('logs/'))
        self.assertTrue(args[1].endswith('_ruuvi_gw_uart.log'))
        self.assertEqual(kwargs, {'console_output': True})

    def assert_rejected_before_work(self, args, message):
        with self.assertRaises(SystemExit) as result:
            self.run_flash(*args)
        self.assertEqual(result.exception.code, 1)
        self.error.assert_called_with(message)
        self.available_ports.assert_not_called()
        self.download.assert_not_called()
        self.run_process.assert_not_called()
        self.log_serial.assert_not_called()

    def test_rejects_compile_and_flash_with_both_slots_before_any_work(self):
        for existing_build in [False, True]:
            if existing_build:
                self.create_images('build')
            for options in [
                    ['--compile_and_flash', '--flash_both_slots'],
                    ['--flash_both_slots', '--compile_and_flash']]:
                with self.subTest(options=options, existing_build=existing_build):
                    self.assert_rejected_before_work(
                        ['build', *options],
                        "Arguments '--flash_both_slots' and '--compile_and_flash' are mutually exclusive.")

    def test_rejects_both_slots_with_non_flashing_modes(self):
        for args in [
                ('-',), ('-', '--erase_flash'), ('-', '--reset'), ('-', '--log_uart'),
                ('-', '--log_to_console'), ('-', '--print_port'),
                ('build', '--compile_only'), ('build', '--print_port'),
                ('v1.17.2-prod', '--download_only'), ('v1.17.2-prod', '--print_port')]:
            with self.subTest(args=args):
                self.assert_rejected_before_work(
                    ['--flash_both_slots', *args],
                    "Argument '--flash_both_slots' requires a flashing command with a firmware version or 'build'.")

    def test_existing_argument_validation(self):
        cases = [
            (['-'],
             "Nothing to do: '-' is passed as 'fw_ver' but '--erase_flash', '--reset', '--log_uart' or '--print_port' is not set"),
            (['-', '--erase_flash', '--download_only'],
             "Argument '--download_only' requires 'fw_ver' to be set."),
            (['build', '--download_only'],
             "Argument '--download_only' requires 'fw_ver' to be set."),
            (['v1.17.2-prod', '--erase_flash', '--download_only'],
             "Arguments '--download_only' and '--erase_flash' are mutually exclusive."),
            (['v1.17.2-prod', '--flash_gw_cfg_def', '--download_only'],
             "Arguments '--flash_gw_cfg_def' and '--download_only' are mutually exclusive."),
            (['-', '--reset', '--flash_gw_cfg_def'],
             "Argument '--flash_gw_cfg_def' requires 'fw_ver' to be set to a firmware version or 'build'."),
            (['build', '--erase_flash', '--compile_and_flash'],
             "'--erase_flash' and '--compile_and_flash' cannot be used together."),
            (['build', '--compile_only', '--compile_and_flash'],
             "'--compile_only' and '--compile_and_flash' cannot be used together."),
            (['build', '--flash_gw_cfg_def', '--compile_only'],
             "'--flash_gw_cfg_def' and '--compile_only' cannot be used together."),
            (['v1.17.2-prod', '--erase_flash', '--log_uart'],
             "'--erase_flash' and '--log_uart' cannot be used together."),
            (['v1.17.2-prod', '--erase_flash', '--log_to_console'],
             "'--erase_flash' and '--log_uart' cannot be used together."),
            (['v1.17.2-prod', '--compile_and_flash'],
             "'--compile_and_flash' must be used with 'build' as fw_ver."),
            (['v1.17.2-prod', '--compile_only'],
             "'--compile_only' must be used with 'build' as fw_ver."),
        ]
        for args, message in cases:
            with self.subTest(args=args):
                self.assert_rejected_before_work(args, message)

    def test_boolean_flags_default_to_false(self):
        arguments = self.parse_arguments('v1.17.2-prod')
        self.assertFalse(arguments.flash_both_slots)
        self.assertFalse(arguments.compile_and_flash)
        self.assertFalse(arguments.flash_gw_cfg_def)

        arguments = self.parse_arguments('v1.17.2-prod', '--flash_both_slots')
        self.assertTrue(arguments.flash_both_slots)
        self.assertFalse(arguments.compile_and_flash)

    def test_non_flashing_commands_remain_available_without_both_slots(self):
        for option in ['--erase_flash', '--reset', '--log_uart', '--log_to_console', '--print_port']:
            with self.subTest(option=option):
                self.log_serial.reset_mock()
                if option == '--print_port':
                    output = io.StringIO()
                    with redirect_stdout(output), self.assertRaises(SystemExit) as result:
                        self.run_flash('-', option)
                    self.assertEqual(result.exception.code, 0)
                    self.assertEqual(output.getvalue(), '/dev/ttyUSB0\n')
                    self.run_process.assert_not_called()
                else:
                    commands = self.run_flash('-', option)
                    expected_operation = {'--erase_flash': 'erase_flash', '--reset': 'run'}.get(option)
                    if expected_operation:
                        self.assertEqual(len(commands), 1)
                        self.assertEqual(commands[0][-1], expected_operation)
                        self.log_serial.assert_not_called()
                    else:
                        self.assertEqual(commands, [])
                        self.log_serial.assert_called_once()
                        self.assertEqual(self.log_serial.call_args.kwargs['console_output'],
                                         option == '--log_to_console')
                self.download.assert_not_called()

    def test_download_only_does_not_flash(self):
        self.available_ports.return_value = []
        autodetect = self.patch('autodetect_serial_port')
        commands = self.run_flash('v1.17.2-prod', '--download_only', port=None)
        self.download.assert_called_once_with('v1.17.2-prod')
        autodetect.assert_not_called()
        self.assertEqual(commands, [])

    def test_compile_only_does_not_flash(self):
        os.mkdir('build')
        with self.assertRaises(SystemExit) as result:
            self.run_flash('build', '--compile_only')
        self.assertEqual(result.exception.code, 0)
        self.assertEqual(self.run_process.call_args_list, [
            call(['ninja', 'ruuvi_gateway_esp.elf']), call(['ninja', '.bin_timestamp']),
        ])
        self.download.assert_not_called()

    def test_compile_only_without_build_directory_fails(self):
        with self.assertRaises(SystemExit) as result:
            self.run_flash('build', '--compile_only')
        self.assertEqual(result.exception.code, 1)
        self.run_process.assert_not_called()

    def test_build_failures_prevent_flashing(self):
        for failed_command, options in [
                (['idf.py', 'build'], ['--flash_both_slots']),
                (['ninja', 'ruuvi_gateway_esp.elf'], ['--compile_and_flash']),
                (['ninja', '.bin_timestamp'], ['--compile_and_flash'])]:
            with self.subTest(failed_command=failed_command):
                self.create_images('build')

                def fail_build(command):
                    if command == failed_command:
                        raise SystemExit(7)

                self.run_process.side_effect = fail_build
                try:
                    with self.assertRaises(SystemExit) as result:
                        self.run_flash('build', *options)
                    self.assertEqual(result.exception.code, 7)
                    self.assertEqual(self.run_process.call_args.args[0], failed_command)
                    self.assertFalse(any('write_flash' in item.args[0]
                                         for item in self.run_process.call_args_list))
                finally:
                    # A failed incremental build leaves main() inside the build directory.
                    os.chdir(self.work_dir)

    def test_download_failure_prevents_flashing(self):
        self.download.side_effect = SystemExit(1)
        with self.assertRaises(SystemExit) as result:
            self.run_flash('v1.17.2-prod', '--flash_both_slots')
        self.assertEqual(result.exception.code, 1)
        self.run_process.assert_not_called()

    def test_flash_failure_prevents_reset_and_logging(self):
        self.create_images('.releases/v1.17.2-prod')
        self.run_process.side_effect = SystemExit(2)
        with self.assertRaises(SystemExit) as result:
            self.run_flash('v1.17.2-prod', '--flash_both_slots', '--reset', '--log_uart')
        self.assertEqual(result.exception.code, 2)
        self.run_process.assert_called_once()
        self.assertIn('write_flash', self.run_process.call_args.args[0])
        self.log_serial.assert_not_called()

    def test_missing_build_tool_prevents_flashing(self):
        self.which.side_effect = lambda tool: None if tool == 'idf.py' else f'/usr/bin/{tool}'
        with self.assertRaises(SystemExit) as result:
            self.run_flash('build', '--flash_both_slots')
        self.assertEqual(result.exception.code, 1)
        self.error.assert_called_with('idf.py not found.')
        self.run_process.assert_not_called()

    def test_missing_esptool_prevents_flashing(self):
        self.which.side_effect = None
        self.which.return_value = None
        with self.assertRaises(SystemExit) as result:
            self.run_flash('v1.17.2-prod', '--flash_both_slots')
        self.assertEqual(result.exception.code, 1)
        self.error.assert_called_with('esptool.py is not installed.')
        self.run_process.assert_not_called()

    def test_esptool_executable_fallback(self):
        self.create_images('.releases/v1.17.2-prod')
        self.which.side_effect = lambda tool: None if tool == 'esptool.py' else f'/usr/bin/{tool}'

        commands = self.run_flash('v1.17.2-prod', '--flash_both_slots')

        self.assertEqual(len(commands), 1)
        self.assertEqual(self.flash_files(commands, esptool='esptool'),
                         self.expected_files('.releases/v1.17.2-prod', True))

    def test_continuing_after_missing_tool_warning(self):
        self.create_images('build')
        self.ask.return_value = True
        for tool in ['esptool', 'idf.py']:
            with self.subTest(tool=tool):
                self.ask.reset_mock()
                missing_tools = ['esptool.py', 'esptool'] if tool == 'esptool' else ['idf.py']
                self.which.side_effect = lambda name: None if name in missing_tools else f'/usr/bin/{name}'

                commands = self.run_flash('build', '--flash_both_slots')

                self.ask.assert_called_once_with()
                self.assertEqual(commands[:-1], [['idf.py', 'build']])
                self.assertEqual(self.flash_files(commands), self.expected_files('build', True))

    def test_explicit_port_takes_precedence_over_environment(self):
        os.environ['RUUVI_GW_SERIAL_PORT'] = '/dev/ttyUSB1'
        self.create_images('.releases/v1.17.2-prod')

        commands = self.run_flash('v1.17.2-prod', '--flash_both_slots')

        self.assertEqual(self.flash_files(commands), self.expected_files('.releases/v1.17.2-prod', True))
        self.ask.assert_not_called()

    def test_environment_port_is_used_when_no_explicit_port(self):
        os.environ['RUUVI_GW_SERIAL_PORT'] = '/dev/ttyUSB1'
        self.available_ports.return_value = ['/dev/ttyUSB0', '/dev/ttyUSB1']
        self.create_images('.releases/v1.17.2-prod')

        commands = self.run_flash('v1.17.2-prod', '--flash_both_slots', port=None)

        self.assertEqual(self.flash_files(commands, port='/dev/ttyUSB1'),
                         self.expected_files('.releases/v1.17.2-prod', True))
        self.ask.assert_not_called()

    def test_unavailable_selected_port_requires_confirmation(self):
        self.available_ports.return_value = []
        self.create_images('.releases/v1.17.2-prod')
        for from_environment in [False, True]:
            for proceed in [False, True]:
                with self.subTest(from_environment=from_environment, proceed=proceed):
                    self.ask.reset_mock()
                    self.download.reset_mock()
                    self.ask.return_value = proceed
                    port = None if from_environment else '/dev/ttyUSB0'
                    if from_environment:
                        os.environ['RUUVI_GW_SERIAL_PORT'] = '/dev/ttyUSB0'

                    if proceed:
                        commands = self.run_flash('v1.17.2-prod', '--flash_both_slots', port=port)
                        self.assertEqual(self.flash_files(commands),
                                         self.expected_files('.releases/v1.17.2-prod', True))
                    else:
                        with self.assertRaises(SystemExit) as result:
                            self.run_flash('v1.17.2-prod', '--flash_both_slots', port=port)
                        self.assertEqual(result.exception.code, 1)
                        self.run_process.assert_not_called()
                        self.download.assert_not_called()
                    self.ask.assert_called_once_with()

    def test_port_autodetection(self):
        self.create_images('.releases/v1.17.2-prod')
        for ports in [[], ['/dev/ttyUSB0'], ['/dev/ttyUSB0', '/dev/ttyUSB1']]:
            with self.subTest(ports=ports):
                self.available_ports.return_value = ports
                if len(ports) == 1:
                    commands = self.run_flash('v1.17.2-prod', '--flash_both_slots', port=None)
                    self.assertEqual(self.flash_files(commands),
                                     self.expected_files('.releases/v1.17.2-prod', True))
                else:
                    with self.assertRaises(SystemExit) as result:
                        self.run_flash('v1.17.2-prod', '--flash_both_slots', port=None)
                    self.assertEqual(result.exception.code, 1)
                    self.run_process.assert_not_called()

    def test_execution_log_is_created_in_requested_directory(self):
        self.create_images('.releases/v1.17.2-prod')
        os.mkdir('logs')

        commands = self.run_flash('v1.17.2-prod', '--flash_both_slots', '--log', '--log_dir', 'logs')

        self.logger.addHandler.assert_called_once()
        handler = self.logger.addHandler.call_args.args[0]
        self.addCleanup(handler.close)
        self.assertEqual(os.path.dirname(handler.baseFilename), os.path.join(self.work_dir, 'logs'))
        self.assertTrue(handler.baseFilename.endswith('_ruuvi_gw_flash.log'))
        self.assertTrue(os.path.isfile(handler.baseFilename))
        self.assertEqual(self.flash_files(commands), self.expected_files('.releases/v1.17.2-prod', True))


if __name__ == '__main__':
    unittest.main()
