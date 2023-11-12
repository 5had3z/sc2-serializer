#include "data.hpp"
#include "database.hpp"

#include <pybind11/functional.h>// Include this header for Pybind11 string conversions
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <bitset>

namespace py = pybind11;

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

            for (std::size_t i = 0; i < img._h * img._w / 8; ++i) {
                auto b = img._data[i];
                std::bitset<8> bitset = std::bitset<8>(std::to_integer<int>(b));
#pragma unroll
                for (std::size_t j = 0; j < 8; ++j) { unpacked_data[j + i * 8] = bitset[j]; }
            }
            py::dtype dtype = py::dtype::of<bool>();// NumPy boolean dtype
            py::array_t<uint8_t> array = py::array_t<uint8_t>({ img._h, img._w }, unpacked_data.data());
            return array;
        });
}


void bindEnums(py::module &m)
{
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

    py::enum_<cvt::Action::Target_Type>(m, "ActionTargetType")
        .value("Self", cvt::Action::Target_Type::Self)
        .value("OtherUnit", cvt::Action::Target_Type::OtherUnit)
        .value("Position", cvt::Action::Target_Type::Position)
        .export_values();

    py::enum_<cvt::Visibility>(m, "Visibility")
        .value("Visible", cvt::Visibility::Visible)
        .value("Snapshot", cvt::Visibility::Snapshot)
        .value("Hidden", cvt::Visibility::Hidden)
        .export_values();
}

PYBIND11_MODULE(sc2_replay_reader, m)
{
    m.doc() = "pbdoc(Python bindings for Starcraft II database reader)pbdoc";

    bindEnums(m);

    bindImage<std::uint8_t>(m, "Image_uint8");
    bindBoolImage(m, "Image_bool");

    py::class_<cvt::Action::Target>(m, "ActionTarget")
        .def(py::init<>())
        .def_readwrite("point", &cvt::Action::Target::point)
        .def_readwrite("other", &cvt::Action::Target::other);

    py::class_<cvt::Action>(m, "Action")
        .def(py::init<>())
        .def_readwrite("unit_ids", &cvt::Action::unit_ids)
        .def_readwrite("ability_id", &cvt::Action::ability_id)
        .def_readwrite("target_type", &cvt::Action::target_type)
        .def_readwrite("target", &cvt::Action::target);

    py::class_<cvt::Point3f>(m, "Point3f", py::buffer_protocol())
        .def(py::init<>())
        .def_readwrite("x", &cvt::Point3f::x)
        .def_readwrite("y", &cvt::Point3f::y)
        .def_readwrite("z", &cvt::Point3f::z)
        .def_buffer([](cvt::Point3f &d) -> py::buffer_info {
            return py::buffer_info(reinterpret_cast<float *>(&d),
                sizeof(float),
                py::format_descriptor<float>::format(),
                1,
                { 3 },
                { sizeof(float) });
        });

    py::class_<cvt::Point2d>(m, "Point2d", py::buffer_protocol())
        .def(py::init<>())
        .def_readwrite("x", &cvt::Point2d::x)
        .def_readwrite("y", &cvt::Point2d::y)
        .def_buffer([](cvt::Point2d &d) -> py::buffer_info {
            return py::buffer_info(reinterpret_cast<int *>(&d),
                sizeof(int),
                py::format_descriptor<int>::format(),
                1,
                { 2 },
                { sizeof(int) });
        });

    py::class_<cvt::Unit>(m, "Unit")
        .def(py::init<>())
        .def_readwrite("id", &cvt::Unit::id)
        .def_readwrite("tgtId", &cvt::Unit::tgtId)
        .def_readwrite("observation", &cvt::Unit::observation)
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
        .def_readwrite("is_powered", &cvt::Unit::is_powered)
        .def(
            "as_array",
            [](const cvt::Unit &data, bool onehot_enum) -> py::array_t<float> {
                return py::cast(cvt::vectorize<float>(data, onehot_enum));
            },
            py::kw_only(),
            py::arg("onehot_enum") = false);


    py::class_<cvt::NeutralUnit>(m, "NeutralUnit")
        .def(py::init<>())
        .def_readwrite("id", &cvt::NeutralUnit::id)
        .def_readwrite("unitType", &cvt::NeutralUnit::unitType)
        .def_readwrite("observation", &cvt::NeutralUnit::observation)
        .def_readwrite("health", &cvt::NeutralUnit::health)
        .def_readwrite("health_max", &cvt::NeutralUnit::health_max)
        .def_readwrite("pos", &cvt::NeutralUnit::pos)
        .def_readwrite("heading", &cvt::NeutralUnit::heading)
        .def_readwrite("radius", &cvt::NeutralUnit::radius)
        .def_readwrite("contents", &cvt::NeutralUnit::contents)
        .def(
            "as_array",
            [](const cvt::NeutralUnit &data, bool onehot_enum) -> py::array_t<float> {
                return py::cast(cvt::vectorize<float>(data, onehot_enum));
            },
            py::kw_only(),
            py::arg("onehot_enum") = false);

    // Expose ReplayDatabase class
    py::class_<cvt::ReplayDatabase>(m, "ReplayDatabase")
        .def(py::init<const std::filesystem::path &>())
        .def("open", &cvt::ReplayDatabase::open)
        .def("isFull", &cvt::ReplayDatabase::isFull)
        .def("size", &cvt::ReplayDatabase::size)
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
        .def_readwrite("gameStep", &cvt::ReplayDataSoA::gameStep)
        .def_readwrite("visibility", &cvt::ReplayDataSoA::visibility)
        .def_readwrite("creep", &cvt::ReplayDataSoA::creep)
        .def_readwrite("player_relative", &cvt::ReplayDataSoA::player_relative)
        .def_readwrite("alerts", &cvt::ReplayDataSoA::alerts)
        .def_readwrite("buildable", &cvt::ReplayDataSoA::buildable)
        .def_readwrite("pathable", &cvt::ReplayDataSoA::pathable)
        .def_readwrite("actions", &cvt::ReplayDataSoA::actions)
        .def_readwrite("units", &cvt::ReplayDataSoA::units)
        .def_readwrite("neutralUnits", &cvt::ReplayDataSoA::neutralUnits);

    m.attr("__version__") = "0.0.1";
}