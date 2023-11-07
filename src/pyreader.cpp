#include "data.hpp"
#include "database.hpp"

#include "pybind11/numpy.h"
#include <pybind11/functional.h>// Include this header for Pybind11 string conversions
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;

int add(int i, int j) { return i + j; }

template<typename T> void bindImage(py::module &m, const std::string &name)
{
    py::class_<cvt::Image<T>>(m, name.c_str())
      .def(py::init<int, int>())
      .def_readwrite("_h", &cvt::Image<T>::_h)
      .def_readwrite("_w", &cvt::Image<T>::_w)
      .def_readwrite("_data", &cvt::Image<T>::_data)
      .def("resize", &cvt::Image<T>::resize)
      .def("clear", &cvt::Image<T>::clear)
      .def("size", &cvt::Image<T>::size)
      .def("empty", &cvt::Image<T>::empty)
      .def("data", &cvt::Image<T>::data);
}


void modify_array(py::array_t<double> input_array)
{
    py::buffer_info buf_info = input_array.request();
    double *ptr = static_cast<double *>(buf_info.ptr);

    int X = buf_info.shape[0];

    for (int i = 0; i < X; i++) { ptr[i] = ptr[i] * 2; }
}
template<typename T> py::array_t<T> image_to_array(const cvt::Image<T> &img)
{
    return py::array(py::buffer_info(img.data(), /* Pointer to data (nullptr indicates use the raw buffer allocation) */
      sizeof(T), /* Size of one scalar */
      py::format_descriptor<T>::format(), /* Python struct-style format descriptor */
      2, /* Number of dimensions */
      { img._h, img._w }, /* Buffer dimensions */
      { sizeof(T) * img._w, sizeof(T) } /* Strides (in bytes) for each index */
      ));
}

// Helper function to convert NumPy array to Image
template<typename T> cvt::Image<T> array_to_image(const py::array_t<T> &array)
{
    py::buffer_info buf_info = array.request();
    cvt::Image<T> img;
    img._h = buf_info.shape[0];
    img._w = buf_info.shape[1];
    img._data.resize(buf_info.size * sizeof(T));
    std::memcpy(img.data(), buf_info.ptr, buf_info.size * sizeof(T));
    return img;
}

// Helper function to convert std::vector to NumPy array
template<typename T> py::array_t<T> vector_to_array(const std::vector<T> &vec)
{
    return py::array(vec.size(), vec.data());
}

// Helper function to convert NumPy array to std::vector
template<typename T> std::vector<T> array_to_vector(const py::array_t<T> &array)
{
    py::buffer_info buf_info = array.request();
    T *ptr = static_cast<T *>(buf_info.ptr);
    return std::vector<T>(ptr, ptr + buf_info.size);
}

PYBIND11_MODULE(sc2_replay_reader, m)
{
    m.doc() = "pbdoc(Python bindings for Starcraft II database reader)pbdoc";
    m.def("add", &add, "pbdoc(Add two numbers test)pbdoc");
    m.def("modify_array", &modify_array, "Function to double the values of a NumPy array");

    // Expose Enum
    py::enum_<cvt::Result>(m, "Result")
      .value("Win", cvt::Result::Win)
      .value("Loss", cvt::Result::Loss)
      .value("Tie", cvt::Result::Tie)
      .value("Undecided", cvt::Result::Undecided)
      .export_values();

    py::enum_<cvt::Race>(m, "Race")
      .value("Terran", cvt::Race::Terran)
      .value("Zerg", cvt::Race::Zerg)
      .value("Protoss", cvt::Race::Protoss)
      .value("Random", cvt::Race::Random)
      .export_values();

    bindImage<std::uint8_t>(m, "Image_uint8");

    // Expose ReplayDatabase class
    py::class_<cvt::ReplayDatabase>(m, "ReplayDatabase")
      .def(py::init<const std::filesystem::path &>())
      .def("open", &cvt::ReplayDatabase::open)
      .def("isFull", &cvt::ReplayDatabase::isFull)
      .def("size", &cvt::ReplayDatabase::size)
      .def("addEntry", &cvt::ReplayDatabase::addEntry)
      .def("makeEmptyEntry", &cvt::ReplayDatabase::makeEmptyEntry)
      .def("getEntry", &cvt::ReplayDatabase::getEntry);


    // Expose ReplayDataSoA structure
    py::class_<cvt::ReplayDataSoA>(m, "ReplayDataSoA")
      .def_readwrite("heightMap", &cvt::ReplayDataSoA::heightMap)
      .def_readwrite("replayHash", &cvt::ReplayDataSoA::replayHash)
      .def_readwrite("playerId", &cvt::ReplayDataSoA::playerId)
      .def_readwrite("playerRace", &cvt::ReplayDataSoA::playerRace)
      .def_readwrite("playerResult", &cvt::ReplayDataSoA::playerResult)
      .def_readwrite("playerMMR", &cvt::ReplayDataSoA::playerMMR)
      .def_readwrite("playerAPM", &cvt::ReplayDataSoA::playerAPM)
      .def_readwrite("visibility", &cvt::ReplayDataSoA::visibility)
      .def_readwrite("creep", &cvt::ReplayDataSoA::creep)
      .def_readwrite("player_relative", &cvt::ReplayDataSoA::player_relative)
      .def_readwrite("alerts", &cvt::ReplayDataSoA::alerts)
      .def_readwrite("buildable", &cvt::ReplayDataSoA::buildable)
      .def_readwrite("pathable", &cvt::ReplayDataSoA::pathable)
      .def_readwrite("actions", &cvt::ReplayDataSoA::actions)
      .def_readwrite("units", &cvt::ReplayDataSoA::units);

    m.attr("__version__") = "0.0.1";
}