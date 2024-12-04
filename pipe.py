#!/usr/bin/env python3
import subprocess
import sys
import argparse
import os
from pathlib import Path

DEBUG_DIR = Path("pipe-debug")
RELEASE_DIR = Path("pipe-release")


def _run_cmd(command: str, check=True):
    print(f"running command with check {check}, and cmd: {command}")
    subprocess.run(["bash", "-c", command], check=check)


def _run_cmd_with_pipefail(command: str, check=True):
    print(f"running pipefail command with check {check}, and cmd: {command}")
    # the command is prefixed with set -o pipefail to ensure that the exit code of this script is still 1
    # when the command is e.g. followed by | tee reports/out.txt.
    # otherwise only the last command in the chain is checked
    cmd_with_pipefail = f"set -o pipefail; {command}"
    subprocess.run(["bash", "-c", cmd_with_pipefail], check=check)


def format(fix: bool, check: bool):
    print("#############################clang-format##########################")
    dry_run = "--dry-run" if not fix else ""
    _run_cmd_with_pipefail(
        f"find src -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i {dry_run} -Werror",
        check=check,
    )
    print("success!")


def check_format():
    format(False, True)


def run_format():
    format(True, True)


def _cmake_configure(build_folder, preset: str, extra_options: str = "", check=True):
    _run_cmd(
        f"cmake -S . -B {build_folder} --preset={preset} {extra_options}",
        check,
    )


def _cmake_build(build_folder):
    _run_cmd(f"cmake --build {build_folder} -j 8")


def configure_debug():
    print("#############################build-debug##########################")
    # workaround: configure twice because conan seems to fail sometimes during the first install.
    _cmake_configure(DEBUG_DIR, "debug-sanitizer-coverage", check=False)
    _cmake_configure(DEBUG_DIR, "debug-sanitizer-coverage", check=True)


def configure_release():
    print("#############################build-release##########################")
    # workaround: configure twice because conan seems to fail sometimes during the first install.
    _cmake_configure(RELEASE_DIR, "release", check=False)
    _cmake_configure(RELEASE_DIR, "release", check=True)


def build_debug():
    _cmake_build(DEBUG_DIR)
    print("success!")


def build_release():
    _cmake_build(RELEASE_DIR)
    print("success!")


def test_unit():
    print("#############################test-unit##########################")
    _run_cmd(f"ctest --test-dir {DEBUG_DIR} --output-junit Testing/utest.xml")
    _run_cmd(f"mkdir -p reports/TestResults/")
    _run_cmd(f"cp {DEBUG_DIR}/Testing/utest.xml reports/TestResults/")
    print("success!")


def lint(check=True):
    print("############################clang-tidy#############################")
    # run-clang-tidy must be executed with -clang-tidy-binary clang-tidy to ensure that clang-tidy is used and
    # not the system clang-tidy e.g. clang-tidy-11 when clang-tidy was installed in version 17 from pip.
    _run_cmd_with_pipefail(
        f" run-clang-tidy -p {DEBUG_DIR} -quiet -clang-tidy-binary clang-tidy | tee reports/clang-tidy",
        check=check,
    )
    print("success!")


def coverage():
    print("############################gcovr#############################")
    _run_cmd(
        f"gcovr --object-directory {DEBUG_DIR} --exclude-throw-branches --filter src --xml reports/coverage.xml "
        "--sonarqube reports/coverage_sonarqube.xml --html reports/coverage.html",
    )
    print("sucess!")


if __name__ == "__main__":  #
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "stages",
        help="The stage(s) to run (equals the function name in this script)."
        "If no stage is specified, the complete pipeline is executed.",
        nargs="*",
    )
    arguments = parser.parse_args()

    if len(arguments.stages) == 0:
        subprocess.run("rm -rf reports/*", shell=True, check=True)
        check_format()
        configure_debug()
        build_debug()
        test_unit()
        coverage()
        configure_release()
        build_release()
        lint()

    else:
        for stage in arguments.stages:
            if stage in locals():
                locals()[stage]()
            else:
                raise RuntimeError(f"Error: Stage {stage} is not defined!")
