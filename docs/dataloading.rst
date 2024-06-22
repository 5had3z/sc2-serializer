Dataloading
===========

Other than Python bindings to the C++ Replay Database and Structures, we include some basic dataloading utilities and an example for pytorch.

ReplaySampler
-------------

.. autoclass:: sc2_serializer.sampler.ReplaySampler
    :members:

.. autoclass:: sc2_serializer.sampler.BasicSampler
    :members:

.. autoclass:: sc2_serializer.sampler.SQLSampler
    :members:


PyTorch Example
---------------

.. autoclass:: sc2_serializer.example.SC2Dataset
    :members:

C++ Extensions
--------------

The header files are included in the library distribution so that you can write additional C++ native functions to process data. An example setup of a PyBind11 CMake project is shown below. You can also use pybind11-stubgen like this project to create type hints for your LSP for any extensions you write. An example build script is provided at the bottom.

utilities.cpp

.. code-block:: c++

    #include <pybind11/pybind11.h>
    #include <pybind11/numpy.h>

    #include "sc2_serializer/data_structures/replay_all.hpp"

    namespace py = pybind11;

    [[nodiscard]] auto doSomething(const cvt::StepDataSoA &replayData, std::int64_t index) noexcept -> py::array_t<std::int32_t>
    {
        const auto &unitData = replayData.units[index];
        return py::array_t<std::int32_t>();
    }

    PYBIND11_MODULE(dataset_utils, m)
    {
        m.def("do_something", &doSomething, "Do something with replay data", py::arg("replay_data"), py::arg("index"));
    }

CMakeLists.txt

.. code-block:: cmake

    cmake_minimum_required(VERSION 3.21.3 FATAL_ERROR)

    project(dataset_utils LANGUAGES CXX)

    include(CPM.cmake)

    execute_process(
    COMMAND
        python3 -c
        "from sc2_serializer import INCLUDE_DIRECTORY; print(INCLUDE_DIRECTORY)"
    OUTPUT_VARIABLE SC2_INCLUDE_DIRECTORY)
    string(STRIP ${SC2_INCLUDE_DIRECTORY} SC2_INCLUDE_DIRECTORY)

    cpmaddpackage(NAME pybind11 GITHUB_REPOSITORY pybind/pybind11 VERSION 2.12.0)

    cpmaddpackage(NAME boost_pfr GITHUB_REPOSITORY boostorg/pfr GIT_TAG 2.1.0)

    cpmaddpackage(GITHUB_REPOSITORY gabime/spdlog VERSION 1.12.0 OPTIONS
                "SPDLOG_BUILD_PIC ON")

    pybind11_add_module(dataset_utils utilities.cpp)
    target_compile_features(dataset_utils PUBLIC cxx_std_23)
    target_link_libraries(dataset_utils PUBLIC Boost::pfr spdlog::spdlog)
    target_include_directories(dataset_utils PUBLIC ${SC2_INCLUDE_DIRECTORY})

build_plugin.sh

.. code-block:: bash

    #!/bin/bash
    cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
    cmake --build build --parallel
    # Compiled extension is hard coded, this may change in different environments
    pybind11-stubgen dataset_utils -o=build --module-path=build/dataset_utils.cpython-310-x86_64-linux-gnu.so
    black build/dataset_utils.pyi
