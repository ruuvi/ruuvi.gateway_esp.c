#!/usr/bin/env python3

import yaml
import argparse
import os


def indent_text(text, prefix='    '):
    """Indent text with a given prefix, applied to each line."""
    if text is None:
        return '\n'
    res = '\n'.join(prefix + line for line in text.strip().split('\n'))
    if not res.endswith('\n'):
        res += '\n'
    return res


def yaml_to_markdown(yaml_file, markdown_file):
    """
    Convert YAML test descriptions to a Markdown document with actions followed by checks.
    """
    with open(yaml_file, 'r') as file:
        test_suites = yaml.safe_load(file)

    with open(markdown_file, 'w') as md:
        for test_set in test_suites['integration_tests']:
            # Write Test Set Title and Description
            md.write(f"## {test_set['test_set_id']}: {test_set['test_set']}\n")
            description = test_set.get('description')
            if description and description.strip():
                md.write(f"{description}\n")
            md.write("\n")
            setup = test_set.get('setup')
            if setup:
                # Set up the Test Set
                md.write("### Setup\n")
                for step in test_set['setup']:
                    md.write(f"- {step['setup_step']}\n")
                    if 'detail' in step and step['detail']:
                        indented_detail = indent_text(step['detail'], prefix='    ')
                        md.write(f"{indented_detail}")
                        md.write("\n")
                md.write("\n")

            for test in test_set['tests']:
                # Write Test ID and Title
                md.write(f"### {test_set['test_set_id']}-{test['test_id']}: {test['title']}\n")
                description = test.get('description')
                if description and description.strip():
                    md.write(f"{description}\n")
                md.write("\n")

                # Setup Steps
                md.write("#### Setup\n")
                for step in test['setup']:
                    md.write(f"- {step['setup_step']}\n")
                    if 'detail' in step and step['detail']:
                        indented_detail = indent_text(step['detail'], prefix='    ')
                        md.write(f"{indented_detail}")
                        md.write("\n")
                md.write("\n")

                # Action and Checks
                md.write("#### Test steps\n")
                for step in test.get('steps', []):
                    md.write(f"- **Action**: {step['action']}\n")
                    if 'detail' in step and step['detail']:
                        md.write("\n")
                        indented_detail = indent_text(step['detail'], prefix='    ')
                        md.write(f"{indented_detail}")
                        md.write("\n")
                    else:
                        md.write("\n")
                    if 'steps' in step:
                        md.write("    **Steps**:\n")
                        md.write("\n")
                        for action_step in step['steps']:
                            md.write(f"    - {action_step}\n")
                        md.write("\n")
                    if 'checks' in step:
                        md.write("    **Checks**:\n")
                        md.write("\n")
                        for check in step['checks']:
                            md.write(f"    - {check}\n")
                        md.write("\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert Ruuvi Gateway Integration Tests YAML file to Markdown.')
    parser.add_argument('input_file', type=str, nargs='?',
                        help='the input YAML file to read')
    args = parser.parse_args()
    if args.input_file is None or args.input_file in ['--help', '-h', '-?']:
        parser.print_help()
        exit(1)
    base_name = os.path.splitext(args.input_file)[0]
    out_file = base_name + '.md'
    yaml_to_markdown(args.input_file, out_file)
