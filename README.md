# sc2-serializer

Starcraft II replay serialization and dataloding tool for machine learning. Serialization uses less than half the memory, runs 11% faster, and produces files 10x smaller than [AlphaStar-Unplugged](https://github.com/google-deepmind/alphastar/tree/main/alphastar/unplugged/data). The associated example outcome and minimap forecasting experiments that use this dataset can be found [here](https://github.com/5had3z/sc2-experiments).

## Documentation

See [https://5had3z.github.io/sc2-serializer/index.html](https://5had3z.github.io/sc2-serializer/index.html) for documentation.

## General Notes

 - If the generated_info.hpp is out-of-date compared to info found in PySC2, re-run scripts/gen_info_header.py.

 - The SC2 API zeros out mineral and vespene resources if they are in the fog-of-war. Instead we default them to the correct value and keep track of their last observed value. Visibility is still included, so it is trivial to revert back to zero'd out observations.

 - Strongly recommended to use [zlib-ng](https://github.com/zlib-ng/zlib-ng) with their LD_PRELOAD instructions for faster (de)serialization.

## Building C++ Observer and Tests

### General

To use the StarCraftII replay observer for converting replays, you will need to initialize the 3rdparty submodule(s).
```bash
git submodule update --init --recursive
```


### Linux

Compilation requires >=gcc-13 since some c++23 features are used. If using ubuntu 18.04 or higher, you can get this via the test toolchain ppa on ubuntu shown below. To update cmake to latest and greatest, follow the instructions [here](https://apt.kitware.com/).

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-13 g++-13
```

To build if you use vscode you should be able to just use the cmake extension and select gcc-13 toolchain. Otherwise the CLI should be the following.

```bash
CC=/usr/bin/gcc-13 CXX=/usr/bin/g++-13 cmake -B build -GNinja
cmake --build build
```

### Windows

Only tested with Visual Studio 2022 Version 17.7.6, _MSC_VER=1936. Downloading and installing TBB from intel is also required, [link here](https://www.intel.com/content/www/us/en/developer/articles/tool/oneapi-standalone-components.html#onetbb) (Tested with 2021.11.0). You will need to make sure `PYTHONHOME` is set correctly. This package will compile both boost and zlib for you. If you already have these somewhere else and want to save the compilation time, you'll have to modify CMakeLists.txt yourself. Otherwise you should be able to compile this library with standard cmake commands or the vscode extension.

```shell
cmake -B build
cmake --build build
```


## Building Python Bindings

Currently requires >=gcc-11 and should be relatively simple to install since CMake Package Manager deals with C++ dependencies, however you will have to install `libboost-iostreams-dev` when building for linux. The library bindings module is called `sc2_serializer` and includes a few extra dataset sampling utilities and an example PyTorch dataloader for outcome prediction.
```bash
sudo apt install libboost-iostreams-dev
pip3 install git+https://github.com/5had3z/sc2-serializer.git
```

If you clone this repo and install with editable mode, you won't get the auto-gen stubs, you can add this manually (you need to install my fork from pyproject.toml)
```bash
pip3 install -e .
pybind11-stubgen _sc2_serializer --module-path build/_sc2_serializer.cpython-310-x86_64-linux-gnu.so -o src/sc2_serializer
```

It is faster to iterate while developing by installing in editable mode, removing pip's compiled version `src/sc2_serializer/_sc2_serializer.cpython-310-x86_64-linux-gnu.so` and symbolically linking to `build/_sc2_serializer.cpython-310-x86_64-linux-gnu.so` instead for incremental builds. You will have manually update the stub with the previously mentioned command if API changes are made.

## Building and Viewing Documentation

The sphinx website can be built and run locally with the following commands, then viewed with a web browser at localhost:8000 (assuming 8000 is default port for python http server).

```bash
make -C docs html
python3 -m http.server --directory docs/_build/html
```

## Development Dependencies

The [scripts](./scripts/) folder contains a bunch of utilities used for developing and creating a StarCraftII Dataset. Additional dependencies required by these can be installed with the `dev` option when installing the main library (or you can peek [pyproject.toml](pyproject.toml) and install them manually).

```bash
pip3 install -e .[dev]
```

## Git hooks

The CI will run several checks on the new code pushed to the repository. These checks can also be run locally without waiting for the CI by following the steps below:

1. [install pre-commit](https://pre-commit.com/#install),
2. Install the Git hooks by running `pre-commit install`.

Once those two steps are done, the Git hooks will be run automatically at every new commit. The Git hooks can also be run manually with `pre-commit run --all-files`, and if needed they can be skipped (not recommended) with `git commit --no-verify`.

> Note: when you commit, you may have to explicitly run `pre-commit run --all-files` twice to make it pass, as each formatting tool will first format the code and fail the first time but should pass the second time.
