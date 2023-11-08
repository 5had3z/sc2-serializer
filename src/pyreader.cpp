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
      .def_property_readonly("data", [](const cvt::Image<T> &img) {
          py::dtype dtype = py::dtype::of<T>();
          const T *data_ptr = reinterpret_cast<const T *>(img._data.data());
          py::array_t<T> array({ img._h, img._w }, data_ptr);
          return array;
      });
}

void bindBoolImage(py::module &m, const std::string &name)
{
    py::class_<cvt::Image<bool>>(m, name.c_str())
      .def(py::init<int, int>())
      .def_property_readonly("data", [](const cvt::Image<bool> &img) {
          std::vector<uint8_t> unpacked_data(img._h * img._w, 0);

          for (std::size_t i = 0; i < img._h * img._w; ++i) {
              // Extract the entire byte containing the boolean
              uint8_t byte_value = static_cast<uint8_t>(img._data[i / 8]);
              // Use bitwise AND and left shift to unpack the boolean
              unpacked_data[i] = (byte_value & (1 << (i % 8))) ? 1 : 0;
          }
          py::dtype dtype = py::dtype::of<bool>();// NumPy boolean dtype
          py::array_t<uint8_t> array = py::array_t<uint8_t>({ img._h, img._w }, unpacked_data.data());
          return array;
      });
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

    py::enum_<cvt::Alliance>(m, "Alliance")
      .value("Self", cvt::Alliance::Self)
      .value("Ally", cvt::Alliance::Ally)
      .value("Neutral", cvt::Alliance::Neutral)
      .value("Enemy", cvt::Alliance::Enemy)
      .export_values();


    py::enum_<cvt::CloakState>(m, "CloakState")
      .value("Unknown", cvt::CloakState::Unknown)
      .value("Cloaked", cvt::CloakState::Cloaked)
      .value("Detected", cvt::CloakState::Detected)
      .value("UnCloaked", cvt::CloakState::UnCloaked)
      .value("Allied", cvt::CloakState::Allied)
      .export_values();


    bindImage<std::uint8_t>(m, "Image_uint8");
    bindBoolImage(m, "Image_bool");

    py::class_<cvt::Action::Target>(m, "ActionTarget")
      .def(py::init<>())
      .def_readwrite("point", &cvt::Action::Target::point)
      .def_readwrite("other", &cvt::Action::Target::other);

    py::enum_<cvt::Action::Target_Type>(m, "ActionTargetType")
      .value("Self", cvt::Action::Target_Type::Self)
      .value("OtherUnit", cvt::Action::Target_Type::OtherUnit)
      .value("Position", cvt::Action::Target_Type::Position);

    py::class_<cvt::Action>(m, "Action")
      .def(py::init<>())
      .def_readwrite("unit_ids", &cvt::Action::unit_ids)
      .def_readwrite("ability_id", &cvt::Action::ability_id)
      .def_readwrite("target_type", &cvt::Action::target_type)
      .def_readwrite("target", &cvt::Action::target);

    py::class_<cvt::Unit>(m, "Unit")
      .def(py::init<>())
      .def_readwrite("id", &cvt::Unit::id)
      .def_readwrite("tgtId", &cvt::Unit::tgtId)
      .def_readwrite("alliance", &cvt::Unit::alliance)
      .def_readwrite("cloak_state", &cvt::Unit::cloak_state)
      .def_readwrite("unitType", &cvt::Unit::unitType)
      .def_readwrite("health", &cvt::Unit::health)
      .def_readwrite("health_max", &cvt::Unit::health_max)
      .def_readwrite("shield", &cvt::Unit::shield)
      .def_readwrite("shield_max", &cvt::Unit::shield_max)
      .def_readwrite("energy", &cvt::Unit::energy)
      .def_readwrite("energy_max", &cvt::Unit::energy_max)
      .def_readwrite("cargo", &cvt::Unit::cargo)
      .def_readwrite("cargo_max", &cvt::Unit::cargo_max)
      .def_readwrite("pos", &cvt::Unit::pos)
      .def_readwrite("heading", &cvt::Unit::heading)
      .def_readwrite("radius", &cvt::Unit::radius)
      .def_readwrite("build_progress", &cvt::Unit::build_progress)
      .def_readwrite("is_blip", &cvt::Unit::is_blip)
      .def_readwrite("is_flying", &cvt::Unit::is_flying)
      .def_readwrite("is_burrowed", &cvt::Unit::is_burrowed)
      .def_readwrite("is_powered", &cvt::Unit::is_powered);


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