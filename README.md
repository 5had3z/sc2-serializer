# sc2-serializer
Alternative to generating the "100's of TB" format from alphastar unplugged.

## Building C++ Observer and Tests

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

## Building Python Bindigs

Currently requires >=gcc-11 and should be relatively simple to install since CMake Package Manager deals with C++ dependencies. The library bindings are called `sc2_replay_reader` as that's mainly what it does.

```
pip3 install .
```
