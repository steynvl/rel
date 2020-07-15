import sys
import os
import re
from typing import List
from command import Command

fail_count = 0
failure = False
REL_CLI = None


class FileInfo:
    def __init__(self, content: List[str], idx: int):
        self.content = content
        self.idx = idx


class RELMatcher:
    find: bool
    group: List[str]

    def __init__(self):
        self.find = False
        self.group = []


def main():
    process_file('test_cases.txt')
    # process_file('perl_cases.txt')


def get_file_contents(fn: str) -> List[str]:
    with open(fn, 'r') as f:
        return [l.strip() for l in f.readlines()]


def grab_line(file_info: FileInfo) -> str:
    line = file_info.content[file_info.idx]
    while line.startswith('//') or len(line) < 1:
        file_info.idx += 1
        assert file_info.idx < len(file_info.content)
        line = file_info.content[file_info.idx]

    file_info.idx += 1
    return line


def explain_failure(pattern: str, data: str, expected: str, actual: str):
    print('-' * 20)
    print(f'Pattern = {pattern}')
    print(f'Data = {data}')
    print(f'Expected = {expected}')
    print(f'Actual = {actual}')


def report(test_name):
    global fail_count
    global failure
    spaces_to_add = 30 - len(test_name)
    buf = [test_name]
    for i in range(spaces_to_add):
        buf.append(' ')

    padded_name = ''.join(buf)
    out = [padded_name, ': ']
    out.append('Passed') if fail_count == 0 else out.append(f'Failed({fail_count})')
    print(''.join(out))

    if fail_count > 0:
        failure = True

    fail_count = 0


def can_compile_regex(pattern: str) -> bool:
    cmd = Command(f'{REL_CLI} "{pattern}"', os.getcwd())
    sig, _, _ = cmd.run()
    return sig == 0


def do_match(pattern: str, string: str) -> 'RELMatcher':
    cmd = Command(f'{REL_CLI} "{pattern}" "{string}"', os.getcwd())
    sig, out, err = cmd.run()
    assert sig == 0

    match_re = re.compile(r'-no match-|match (?:\((\d+),(\d+)\)\s*)+')
    match_result = match_re.search(out)
    assert match_result is not None

    matcher = RELMatcher()
    if match_result.group() == '-no match-':
        return matcher
    else:
        matcher.find = True
        group_indices = list(map(int, re.findall(r'\d+', match_result.group())))
        assert len(group_indices) % 2 == 0
        for i in range(0, len(group_indices), 2):
            matcher.group.append(string[group_indices[i]:group_indices[i+1]])

    return matcher


def process_file(fn: str):
    global fail_count
    file_info = FileInfo(get_file_contents(fn), 0)

    while file_info.idx < len(file_info.content):
        pattern_string = grab_line(file_info)

        if not can_compile_regex(pattern_string):
            _ = grab_line(file_info)
            expected_result = grab_line(file_info)
            if expected_result.startswith('error'):
                continue
            print(f'Could not compile pattern: {pattern_string}')
            fail_count += 1
            continue

        data_string = grab_line(file_info)
        expected_result = grab_line(file_info)

        m = do_match(pattern_string, data_string)

        result = []
        if m.find:
            result.append('true ')
            result.append(m.group[0])
            result.append(' ')
            result.append(expected_result.split()[2])
        else:
            result.append('false ')
            result.append(expected_result.split()[1])

        if m.find:
            for i in range(1, len(m.group)):
                result.append(' ')
                result.append(m.group[i])

        result = ''.join(result)
        if result != expected_result:
            explain_failure(pattern_string, data_string, expected_result, result)
            fail_count += 1

    report(fn)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python3 run_regex_test.py /path/to/rel')
        sys.exit(1)

    REL_CLI = sys.argv[1]
    fail_count = 0
    failure = False

    sys.exit(main())
