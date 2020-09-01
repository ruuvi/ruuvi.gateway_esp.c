import os
import sys
import argparse
import re
import xml.etree.ElementTree as xml_et
from xml.dom import minidom

assert sys.version_info >= (3, 5)


def scan_cpp(base_path, rel_project_path):
    test_project_path = os.path.join(base_path, rel_project_path)
    if not os.path.isdir(test_project_path):
        print('Error: there is no folder: %s' % test_project_path)
        sys.exit(1)
    dict_test_suites = dict()
    re_line = re.compile(r'^class\s+([\w\d_]+)\s*:\s*public\s*::testing::Test')
    for dirpath, dirnames, filenames in os.walk(test_project_path):
        for file in filenames:
            if os.path.splitext(file)[1] == '.cpp':
                cpp_file_path = os.path.join(dirpath, file)
                print('    Found: %s' % os.path.relpath(cpp_file_path))
                with open(cpp_file_path) as fd:
                    for line in fd.readlines():
                        m = re_line.match(line)
                        if m:
                            test_suit_name = m.group(1)
                            rel_path = os.path.relpath(cpp_file_path, base_path)
                            dict_test_suites[test_suit_name] = rel_path
                            break
    return dict_test_suites


def handle_gtest_results(input_file, base_path, rel_project_path, xml_root):
    dict_test_suites = scan_cpp(base_path, rel_project_path)
    with open(input_file, 'r') as fd:
        input_content = fd.read()
    gtest_xml = xml_et.XML(input_content)
    if gtest_xml.tag != 'testsuites':
        print('Error: Bad input file: expected "testsuites" section, but found "%s"' % gtest_xml.tag)
        sys.exit(1)
    test_suits = list(gtest_xml)
    for test_suite in test_suits:
        assert isinstance(test_suite, xml_et.Element)
        if test_suite.tag != 'testsuite':
            print('Error: Bad input file: expected "testsuite" section, but found "%s"' % test_suite.tag)
            sys.exit(1)
        test_suite_name = test_suite.attrib['name']
        print('    Test suite name: %s' % test_suite_name)
        test_suite_cpp = dict_test_suites[test_suite_name]
        xml_elem_file = xml_et.Element('file')
        xml_elem_file.set('path', test_suite_cpp)
        test_cases = list(test_suite)
        for test_case in test_cases:
            assert isinstance(test_case, xml_et.Element)
            if test_case.tag != 'testcase':
                print('Error: Bad input file: expected "testcase" section, but found "%s"' % test_case.tag)
                sys.exit(1)
            test_case_name = test_case.attrib['name']
            test_case_time = test_case.attrib['time']
            test_case_time = str(int(float(test_case_time) * 1000))
            test_case_status = test_case.attrib['status']
            test_case_result = test_case.attrib['result']
            xml_elem_test_case = xml_et.Element('testCase')
            print('        Test case: %s: %s: %s' % (test_case_name, test_case_status, test_case_result))
            xml_elem_test_case.set('name', test_case_name)
            xml_elem_test_case.set('duration', test_case_time)
            if test_case_status == 'run':
                if test_case_result == 'completed':
                    failures = list(test_case)
                    for failure in failures:
                        assert isinstance(failure, xml_et.Element)
                        if failure.tag != 'failure':
                            print('Error: Bad input file: expected "failure" section, but found "%s"' % failure.tag)
                            sys.exit(1)
                        failure_message = failure.attrib['message']
                        failure_text = failure.text
                        failure_message_short = failure_message.partition('\n')[0]
                        print('            Failure: %s' % failure_message_short)
                        xml_elem_failure = xml_et.Element('failure')
                        # failure_message = failure_message.replace('\n', '&#x0A')
                        xml_elem_failure.set('message', failure_message)
                        xml_elem_test_case.append(xml_elem_failure)
                    pass
                else:
                    print('Error: Unknown test case result in status "run": %s' % test_case_result)
                    sys.exit(1)
            elif test_case_status == 'notrun':
                if test_case_result == 'suppressed':
                    # <skipped message="short message">other</skipped>
                    xml_elem_skipped = xml_et.Element('skipped')
                    xml_elem_skipped.set('message', 'disabled')
                    xml_elem_test_case.append(xml_elem_skipped)
                else:
                    print('Error: Unknown test case result in status "notrun": %s' % test_case_result)
                    sys.exit(1)
            else:
                print('Error: Unknown test case status: %s' % test_case_status)
                sys.exit(1)
            xml_elem_file.append(xml_elem_test_case)
            pass

        xml_root.append(xml_elem_file)


def main():
    parser = argparse.ArgumentParser(
        description='Convert google-test result xml to the sonarcloud generic execution report xml')
    parser.add_argument('--cmake_build_path', help='CMake build path')
    parser.add_argument('--base_path', help='base path', default='.')
    parser.add_argument('--output', help='output xml', required=True)
    parser.add_argument('--update_sonar_project_properties',
                        help='Update "sonar.tests" property in "sonar-project.properties"',
                        action='store_true')
    args = parser.parse_args()
    cmake_build_path = args.cmake_build_path
    base_path = args.base_path
    output_file = args.output
    list_of_test_projects = list()

    xml_root = xml_et.Element('testExecutions')
    xml_root.set('version', '1')

    tests_path = os.path.split(os.path.abspath(cmake_build_path))[0]

    for dirpath, dirnames, filenames in os.walk(cmake_build_path):
        for file in filenames:
            if file == 'gtestresults.xml':
                project_name = os.path.split(dirpath)[1]
                project_path = os.path.join(tests_path, project_name)
                rel_project_path = os.path.relpath(project_path, base_path)
                list_of_test_projects.append(rel_project_path)
                print('Found google-test results: %s' % os.path.join(dirpath, file))
                handle_gtest_results(os.path.join(dirpath, file), base_path, rel_project_path, xml_root)

    xml_str = minidom.parseString(xml_et.tostring(xml_root)).toprettyxml(indent="   ")
    with open(output_file, 'w') as fd:
        fd.write(xml_str)

    if args.update_sonar_project_properties:
        with open('sonar-project.properties', 'w') as fd:
            fd.write('sonar.tests=%s\n' % ','.join(list_of_test_projects))


if __name__ == '__main__':
    main()
