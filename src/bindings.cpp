#include "database.hpp"
#include "replay_parsing.hpp"

#include <pybind11/functional.h>// Include this header for Pybind11 string conversions
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <optional>

namespace py = pybind11;

template<typename T> void bindImage(py::module &m, const std::string &name)
{
    py::class_<cvt::Image<T>>(m, name.c_str(), py::buffer_protocol())
        .def_property_readonly("shape", [](const cvt::Image<T> &img) { return py::make_tuple(img._h, img._w); })
        .def_property_readonly("data",
            [](const cvt::Image<T> &img) {
                py::array_t<T> out({ img._h, img._w });
                std::ranges::copy(img.as_span(), out.mutable_data());
                return out;
            })
        .def_buffer([](cvt::Image<T> &img) -> py::buffer_info {
            return py::buffer_info(img.data(),
                sizeof(T),
                py::format_descriptor<T>::format(),
                2,
                { img._h, img._w },
                { sizeof(T) * img._h, sizeof(T) });
        });
}

// Specialization for bool image which doesn't have native buffer support
template<typename T>
    requires std::is_same_v<T, bool>
void bindImage(py::module &m, const std::string &name)
{
    py::class_<cvt::Image<bool>>(m, name.c_str())
        .def_property_readonly("shape", [](const cvt::Image<T> &img) { return py::make_tuple(img._h, img._w); })
        .def_property_readonly("data", [](const cvt::Image<bool> &img) {
            py::array_t<std::uint8_t> out({ img._h, img._w });
            unpackBoolImage<std::uint8_t>(img, out.mutable_data());
            return out;
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

    py::enum_<spdlog::level::level_enum>(m, "spdlog_lvl")
        .value("trace", spdlog::level::level_enum::trace)
        .value("debug", spdlog::level::level_enum::debug)
        .value("info", spdlog::level::level_enum::info)
        .value("warn", spdlog::level::level_enum::warn)
        .value("err", spdlog::level::level_enum::err)
        .value("critical", spdlog::level::level_enum::critical)
        .value("off", spdlog::level::level_enum::off)
        .export_values();

    py::enum_<cvt::AddOn>(m, "AddOn")
        .value("NONE", cvt::AddOn::None)
        .value("Reactor", cvt::AddOn::Reactor)
        .value("TechLab", cvt::AddOn::TechLab)
        .export_values();
}

PYBIND11_MODULE(_sc2_replay_reader, m)
{
    m.doc() = "pbdoc(Python bindings for Starcraft II database reader)pbdoc";

    bindEnums(m);

    bindImage<std::uint8_t>(m, "Image_uint8");
    bindImage<bool>(m, "Image_bool");

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

    py::class_<cvt::Action>(m, "Action")
        .def(py::init<>())
        .def_readonly("unit_ids", &cvt::Action::unit_ids)
        .def_readonly("ability_id", &cvt::Action::ability_id)
        .def_readonly("target_type", &cvt::Action::target_type)
        .def_property_readonly("target_point",
            [](const cvt::Action &action) -> std::optional<cvt::Point2d> {
                return action.target_type == cvt::Action::Target_Type::Position ? std::optional{ action.target.point }
                                                                                : std::nullopt;
            })
        .def_property_readonly("target_other", [](const cvt::Action &action) -> std::optional<cvt::UID> {
            return action.target_type == cvt::Action::Target_Type::OtherUnit ? std::optional{ action.target.other }
                                                                             : std::nullopt;
        });

    py::class_<cvt::UnitOrder>(m, "UnitOrder")
        .def_readwrite("ability_id", &cvt::UnitOrder::ability_id)
        .def_readwrite("tgtId", &cvt::UnitOrder::tgtId)
        .def_readwrite("target_pos", &cvt::UnitOrder::target_pos)
        .def_readwrite("progress", &cvt::UnitOrder::progress);

    py::class_<cvt::Score>(m, "Score")
        .def(py::init<>())
        .def_readonly("score_float", &cvt::Score::score_float)
        .def_readonly("idle_production_time", &cvt::Score::idle_production_time)
        .def_readonly("idle_worker_time", &cvt::Score::idle_worker_time)
        .def_readonly("total_value_units", &cvt::Score::total_value_units)
        .def_readonly("total_value_structures", &cvt::Score::total_value_structures)
        .def_readonly("killed_value_units", &cvt::Score::killed_value_units)
        .def_readonly("killed_value_structures", &cvt::Score::killed_value_structures)
        .def_readonly("collected_minerals", &cvt::Score::collected_minerals)
        .def_readonly("collected_vespene", &cvt::Score::collected_vespene)
        .def_readonly("collection_rate_minerals", &cvt::Score::collection_rate_minerals)
        .def_readonly("collection_rate_vespene", &cvt::Score::collection_rate_vespene)
        .def_readonly("spent_minerals", &cvt::Score::spent_minerals)
        .def_readonly("spent_vespene", &cvt::Score::spent_vespene)
        .def_readonly("total_damage_dealt_life", &cvt::Score::total_damage_dealt_life)
        .def_readonly("total_damage_dealt_shields", &cvt::Score::total_damage_dealt_shields)
        .def_readonly("total_damage_dealt_energy", &cvt::Score::total_damage_dealt_energy)
        .def_readonly("total_damage_taken_life", &cvt::Score::total_damage_taken_life)
        .def_readonly("total_damage_taken_shields", &cvt::Score::total_damage_taken_shields)
        .def_readonly("total_damage_taken_energy", &cvt::Score::total_damage_taken_energy)
        .def_readonly("total_healed_life", &cvt::Score::total_healed_life)
        .def_readonly("total_healed_shields", &cvt::Score::total_healed_shields)
        .def_readonly("total_healed_energy", &cvt::Score::total_healed_energy);

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
        .def_readwrite("assigned_harvesters", &cvt::Unit::assigned_harvesters)
        .def_readwrite("ideal_harvesters", &cvt::Unit::ideal_harvesters)
        .def_readwrite("weapon_cooldown", &cvt::Unit::weapon_cooldown)
        .def_readwrite("pos", &cvt::Unit::pos)
        .def_readwrite("heading", &cvt::Unit::heading)
        .def_readwrite("radius", &cvt::Unit::radius)
        .def_readwrite("build_progress", &cvt::Unit::build_progress)
        .def_readwrite("is_blip", &cvt::Unit::is_blip)
        .def_readwrite("is_flying", &cvt::Unit::is_flying)
        .def_readwrite("is_burrowed", &cvt::Unit::is_burrowed)
        .def_readwrite("is_powered", &cvt::Unit::is_powered)
        .def_readwrite("in_cargo", &cvt::Unit::in_cargo)
        .def_readwrite("order0", &cvt::Unit::order0)
        .def_readwrite("order1", &cvt::Unit::order1)
        .def_readwrite("order2", &cvt::Unit::order2)
        .def_readwrite("order3", &cvt::Unit::order3)
        .def_readwrite("buff0", &cvt::Unit::buff0)
        .def_readwrite("buff1", &cvt::Unit::buff1)
        .def_readwrite("add_on_tag", &cvt::Unit::add_on_tag)
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

    // Expose ReplayDataSoA structure
    py::class_<cvt::ReplayDataSoA>(m, "ReplayDataSoA")
        .def_readwrite("heightMap", &cvt::ReplayDataSoA::heightMap)
        .def_readwrite("replayHash", &cvt::ReplayDataSoA::replayHash)
        .def_readwrite("gameVersion", &cvt::ReplayDataSoA::gameVersion)
        .def_readwrite("playerId", &cvt::ReplayDataSoA::playerId)
        .def_readwrite("playerRace", &cvt::ReplayDataSoA::playerRace)
        .def_readwrite("playerResult", &cvt::ReplayDataSoA::playerResult)
        .def_readwrite("playerMMR", &cvt::ReplayDataSoA::playerMMR)
        .def_readwrite("playerAPM", &cvt::ReplayDataSoA::playerAPM)
        .def_readwrite("mapWidth", &cvt::ReplayDataSoA::mapWidth)
        .def_readwrite("mapHeight", &cvt::ReplayDataSoA::mapHeight)
        .def_readwrite("gameStep", &cvt::ReplayDataSoA::gameStep)
        .def_readwrite("minerals", &cvt::ReplayDataSoA::minearals)
        .def_readwrite("vespene", &cvt::ReplayDataSoA::vespene)
        .def_readwrite("popMax", &cvt::ReplayDataSoA::popMax)
        .def_readwrite("popArmy", &cvt::ReplayDataSoA::popArmy)
        .def_readwrite("popWorkers", &cvt::ReplayDataSoA::popWorkers)
        .def_readwrite("score", &cvt::ReplayDataSoA::score)
        .def_readwrite("visibility", &cvt::ReplayDataSoA::visibility)
        .def_readwrite("creep", &cvt::ReplayDataSoA::creep)
        .def_readwrite("player_relative", &cvt::ReplayDataSoA::player_relative)
        .def_readwrite("alerts", &cvt::ReplayDataSoA::alerts)
        .def_readwrite("buildable", &cvt::ReplayDataSoA::buildable)
        .def_readwrite("pathable", &cvt::ReplayDataSoA::pathable)
        .def_readwrite("actions", &cvt::ReplayDataSoA::actions)
        .def_readwrite("units", &cvt::ReplayDataSoA::units)
        .def_readwrite("neutralUnits", &cvt::ReplayDataSoA::neutralUnits);

    // Expose ReplayDatabase class
    py::class_<cvt::ReplayDatabase>(m, "ReplayDatabase")
        .def(py::init<>())
        .def(py::init<const std::filesystem::path &>(), py::arg("dbPath"))
        .def("open", &cvt::ReplayDatabase::open, py::arg("dbPath"))
        .def("isFull", &cvt::ReplayDatabase::isFull)
        .def("size", &cvt::ReplayDatabase::size)
        .def("getEntry", &cvt::ReplayDatabase::getEntry, py::arg("index"))
        .def("getHashIdEntry", &cvt::ReplayDatabase::getHashId, py::arg("index"));

    py::class_<cvt::ReplayParser>(m, "ReplayParser")
        .def(py::init<const std::filesystem::path &>(), py::arg("dataPath"))
        .def("sample", &cvt::ReplayParser::sample, py::arg("timeIdx"), py::arg("unit_alliance") = false)
        .def("parse_replay", &cvt::ReplayParser::parseReplay, py::arg("replayData"))
        .def("size", &cvt::ReplayParser::size)
        .def("empty", &cvt::ReplayParser::empty)
        .def_property_readonly("data", &cvt::ReplayParser::data, py::return_value_policy::reference_internal);

    m.def("setReplayDBLoggingLevel", &cvt::setReplayDBLoggingLevel, py::arg("lvl"));

    m.attr("__version__") = "0.0.1";
}
