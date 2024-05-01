import argparse
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input_file')
    args = parser.parse_args()
    pattern = re.compile('^([DIWE]) \(\d+\) (.*)$')
    with open(args.input_file, "r") as fd:
        while True:
            line = fd.readline()
            if not line:
                break
            m = pattern.match(line)
            if m is None:
                print(f'{line}', end='')
            else:
                print(f'{m.group(1)} {m.group(2)}')


if __name__ == '__main__':
    main()
