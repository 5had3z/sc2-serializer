# [WIP] sc2-serializer
Alternative to generating the "100's of TB" format from alphastar unplugged.

## General Notes

 - If the generated_info.hpp is out-of-date compared to info found in PySC2, re-run scripts/gen_info_header.py.

 - The SC2 API Zeros out the mineral and vesper resources if they are in the fog-of-war. Instead we default them to the correct value and keep track of their last observed value. We still include the visibility so it is trivial to revert back to zero'd out observations.

 - Strongly recommended to use [zlib-ng](https://github.com/zlib-ng/zlib-ng) with their LD_PRELOAD instructions.

## Building C++ Observer and Tests

### Python Dependencies

This library relies on a python script to find the dataVersion for launching replays. This python script uses [mpyq](https://github.com/eagleflo/mpyq), which can be installed with 

```
pip install mpyq
```

You can manually define these variables to choose a python version.

- -DPYTHON_INCLUDE
- -DPYTHON_LIBRARIES

### Linux

This requires >=gcc-13 since some c++23 features are used.
If using ubuntu 18.04 or higher, you can get this via the test toolchain ppa on ubuntu. To update cmake to latest and greatest, follow the instructions [here](https://apt.kitware.com/).

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-13 g++-13
```

To use the SCII replay observer, you will need to initialize the 3rdparty submodule(s).

```
git submodule update --init --recursive
```

To build if you use vscode you should be able to just use the cmake extension and select gcc-13 toolchain. Otherwise the CLI should be the following.

```
CC=/usr/bin/gcc-13 CXX=/usr/bin/g++-13 cmake -B build -GNinja
cmake --build build
```

### Windows

Only tested with Visual Studio 2022 Version 17.7.6, _MSC_VER=1936.

This will compile both boost and zlib for you. 

To use the SCII replay observer, you will need to initialize the 3rdparty submodule(s).

```
git submodule update --init --recursive
```

To build if you use vscode you should be able to just use the cmake extension Otherwise the CLI should be the following.

```
cmake -B build
cmake --build build --target ALL_BUILD
```

## Building Python Bindigs

Currently requires >=gcc-11 and should be relatively simple to install since CMake Package Manager deals with C++ dependencies. The library bindings are called `sc2_replay_reader` as that's mainly what it does.
```sh
pip3 install .
```

If you install in editable mode, you won't get the auto-gen stubs, you can add this manually (you need to install my fork from pyproject.toml)
```sh
pybind11-stubgen _sc2_replay_reader --module-path build/_sc2_replay_reader.cpython-310-x86_64-linux-gnu.so -o src/sc2_replay_reader
```

It is also faster to iterate while developing by installing in editable mode, removing pip's compiled version `src/sc2_replay_reader/_sc2_replay_reader.cpython-310-x86_64-linux-gnu.so` and sybolically linking to `build/_sc2_replay_reader.cpython-310-x86_64-linux-gnu.so` instead for incremental builds. You will have manually update the stub with the previously mentioned script however if api changes are made.

## Generating SQL Database

To generate meta-data for all the replays, we require additional dependencies:

Install additional dependencies with:
```bash
pip install sc2-replay-parser[database]
```

Set the environment variable "DATAPATH" to the directory containing "*.SC2Replays" files.

Run `python gen_database.py --workspace <OUTPUT_DIR> --workers=8`

- <OUTPUT_DIR> is the output directory.
- --workers sets the number of data loader workers.


### Git hooks
The CI will run several checks on the new code pushed to the repository. These checks can also be run locally without waiting for the CI by following the steps below:

1. [install pre-commit](https://pre-commit.com/#install),
2. Install the Git hooks by running `pre-commit install`.

Once those two steps are done, the Git hooks will be run automatically at every new commit. The Git hooks can also be run manually with `pre-commit run --all-files`, and if needed they can be skipped (not recommended) with `git commit --no-verify`.

> Note: when you commit, you may have to explicitly run `pre-commit run --all-files` twice to make it pass, as each formatting tool will first format the code and fail the first time but should pass the second time.
