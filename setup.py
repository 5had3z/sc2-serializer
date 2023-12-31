"""
Compile the python reader only.
Based on https://github.com/pybind/cmake_example/blob/master/setup.py
"""
import os
import subprocess
import sys
from pathlib import Path

import ninja
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name: str, source_dir: str = "") -> None:
        super().__init__(name, sources=[])
        self.source_dir = Path(source_dir).resolve()


class CMakeBuild(build_ext):
    def get_outputs(self):
        return super().get_outputs() + ["_sc2_replay_reader.pyi"]

    def build_extension(self, ext: CMakeExtension) -> None:
        ext_fullpath = Path.cwd() / self.get_ext_fullpath(ext.name)
        extdir = ext_fullpath.parent.resolve()

        cmake_args = [
            f"-DCMAKE_BUILD_TYPE={'Debug' if self.debug else 'Release'}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}{os.sep}",
            f"-DCMAKE_MAKE_PROGRAM:FILEPATH={Path(ninja.BIN_DIR)/'ninja'}",
            "-GNinja",
            "-DSC2_TESTS=OFF",
            "-DSC2_CONVERTER=OFF",
        ]

        build_args = ["--parallel"]

        build_temp = Path(self.build_temp) / ext.name
        if not build_temp.exists():
            build_temp.mkdir(parents=True)

        subprocess.run(
            ["cmake", str(ext.source_dir), *cmake_args], cwd=build_temp, check=True
        )
        subprocess.run(
            ["cmake", "--build", ".", *build_args], cwd=build_temp, check=True
        )
        subprocess.run(
            [
                "pybind11-stubgen",
                "_sc2_replay_reader",
                f"-o={extdir}{os.sep}",
                f"--module-path={ext_fullpath}",
            ],
            cwd=build_temp,
            check=True,
        )
        subprocess.run(
            ["black", f"{extdir}{os.sep}_sc2_replay_reader.pyi"],
            cwd=build_temp,
            check=True,
        )


if __name__ == "__main__":
    setup(
        ext_modules=[CMakeExtension("sc2_replay_reader._sc2_replay_reader")],
        cmdclass={"build_ext": CMakeBuild},
        zip_safe=False,
    )
