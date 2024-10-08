/**
 * @file bindings.cpp
 * @author Bryce Ferenczi
 * @brief Python Bindings for sc2-serializer APIs.
 * @version 0.1
 * @date 2024-05-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "data_structures/replay_all.hpp"
#include "data_structures/replay_minimaps.hpp"
#include "data_structures/replay_scalars.hpp"
#include "database.hpp"
#include "replay_parsing.hpp"

#include <pybind11/functional.h>// Include this header for Pybind11 string conversions
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <spdlog/fmt/ranges.h>

#include <optional>

namespace py = pybind11;

/**
 * @brief Create python bindings to minimap image
 * @tparam T underlying datatype of the image
 * @param m top-level pybind module
 * @param name name of python object
 */
template<typename T> void bindImage(py::module &m, const std::string &name)
{
    py::class_<cvt::Image<T>>(m, name.c_str(), py::buffer_protocol())
        .def_property_readonly("empty", &cvt::Image<T>::empty)
        .def_property_readonly("nbytes", &cvt::Image<T>::size)
        .def_property_readonly("nelem", &cvt::Image<T>::nelem)
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


/**
 * @brief Specialisation of minimap binding for bool image which needs to be transformed to uint8 for numpy usage. Also
 * removes def_buffer implementation since this can't be used.
 * @param m top-level pybind module
 * @param name name of python object
 */
template<> void bindImage<bool>(py::module &m, const std::string &name)
{
    py::class_<cvt::Image<bool>>(m, name.c_str())
        .def_property_readonly("empty", &cvt::Image<bool>::empty)
        .def_property_readonly("nbytes", &cvt::Image<bool>::size)
        .def_property_readonly("nelem", &cvt::Image<bool>::nelem)
        .def_property_readonly("shape", [](const cvt::Image<bool> &img) { return py::make_tuple(img._h, img._w); })
        .def_property_readonly("data", [](const cvt::Image<bool> &img) {
            py::array_t<std::uint8_t> out({ img._h, img._w });
            unpackBoolImage<std::uint8_t>(img, out.mutable_data());
            return out;
        });
}

/**
 * @brief Create python bindings for replay data struct and database.
 * @tparam T underlying replay data struct type
 * @param m top-level pybind module
 * @param name name of python object
 */
template<typename T> void bindReplayDataInterfaces(py::module &m, const std::string &name)
{
    py::class_<T>(m, name.c_str())
        .def_readwrite("header", &T::header)
        .def_readwrite("data", &T::data)
        .def("__len__", &T::size)
        .def("__getitem__", [](const T &t, std::size_t i) {
            if (i >= t.size()) { throw py::index_error(); }
            return t[i];
        });

    const auto dbName = name + "Database";
    py::class_<cvt::ReplayDatabase<T>>(m, dbName.c_str())
        .def(py::init<>())
        .def(py::init<const std::filesystem::path &>(), py::arg("dbPath"))
        .def("open", &cvt::ReplayDatabase<T>::open, py::arg("dbPath"))
        .def("create", &cvt::ReplayDatabase<T>::create, py::arg("dbPath"))
        .def("load", &cvt::ReplayDatabase<T>::load, py::arg("dbPath"))
        .def("isFull", &cvt::ReplayDatabase<T>::isFull)
        .def("size", &cvt::ReplayDatabase<T>::size)
        .def("__len__", &cvt::ReplayDatabase<T>::size)
        .def("addEntry", &cvt::ReplayDatabase<T>::addEntry, py::arg("data"))
        .def("getEntry", &cvt::ReplayDatabase<T>::getEntry, py::arg("index"))
        .def("__getitem__", &cvt::ReplayDatabase<T>::getEntry, py::arg("index"))
        .def("getHeader", &cvt::ReplayDatabase<T>::getHeader, py::arg("index"))
        .def("getEntryUID", &cvt::ReplayDatabase<T>::getEntryUID, py::arg("index"))
        .def_property_readonly("path", &cvt::ReplayDatabase<T>::path);

    const auto parserName = name + "Parser";
    py::class_<cvt::ReplayParser<T>> parser(m, parserName.c_str());
    parser.def(py::init<const std::filesystem::path &>(), py::arg("info_path"))
        .def("sample_all", &cvt::ReplayParser<T>::sampleAll, py::arg("index"), py::arg("unit_alliance") = false)
        .def("parse_replay", &cvt::ReplayParser<T>::parseReplay, py::arg("replay_data"))
        .def("size", &cvt::ReplayParser<T>::size)
        .def("__len__", &cvt::ReplayParser<T>::size)
        .def("empty", &cvt::ReplayParser<T>::empty)
        .def("setPlayerMinimapExpansion", &cvt::ReplayParser<T>::setPlayerMinimapExpansion, py::arg("flag"))
        .def("getPlayerMinimapExpansion", &cvt::ReplayParser<T>::getPlayerMinimapExpansion)
        .def("setMinimapFeatures", &cvt::ReplayParser<T>::setMinimapFeatures, py::arg("features"))
        .def("getMinimapFeatures", &cvt::ReplayParser<T>::getMinimapFeatures)
        .def_property_readonly("data", &cvt::ReplayParser<T>::data, py::return_value_policy::reference_internal)
        .def_property_readonly("info", &cvt::ReplayParser<T>::info, py::return_value_policy::reference_internal);

    using step_data_t = typename T::step_type;

    if constexpr (cvt::HasUnitData<step_data_t>) {
        parser.def("sample_units", &cvt::ReplayParser<T>::sampleUnits, py::arg("index"));
        parser.def("sample_units_group_alliance", &cvt::ReplayParser<T>::sampleUnitsGroupAlliance, py::arg("index"));
        parser.def("sample_neutral_units", &cvt::ReplayParser<T>::sampleNeutralUnits, py::arg("index"));
    }

    if constexpr (cvt::HasMinimapData<step_data_t>) {
        parser.def("sample_minimaps", &cvt::ReplayParser<T>::sampleMinimaps, py::arg("index"));
    }

    if constexpr (cvt::HasScalarData<step_data_t>) {
        parser.def("sample_scalars", &cvt::ReplayParser<T>::sampleScalars, py::arg("index"));
    }

    if constexpr (cvt::HasActionData<step_data_t>) {
        parser.def("sample_actions", &cvt::ReplayParser<T>::sampleActions, py::arg("index"));
    }
}


/**
 * @brief Bind enums from enums.hpp
 * @param m top-level pybind module
 */
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

    py::enum_<cvt::Action::TargetType>(m, "ActionTargetType")
        .value("Self", cvt::Action::TargetType::Self)
        .value("OtherUnit", cvt::Action::TargetType::OtherUnit)
        .value("Position", cvt::Action::TargetType::Position)
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

PYBIND11_MODULE(_sc2_serializer, m)
{
    m.doc() = "pbdoc(Python bindings for Starcraft II database reader)pbdoc";
    m.attr("__version__") = "0.0.1";

    bindEnums(m);

    bindImage<std::uint8_t>(m, "ImageUInt8");
    bindImage<bool>(m, "ImageBool");

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
        })
        .def("__repr__", [](const cvt::Point3f &p) { return fmt::format("Point3f(x={}, y={}, z={})", p.x, p.y, p.z); });

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
        })
        .def("__repr__", [](const cvt::Point2d &p) { return fmt::format("Point2d(x={}, y={})", p.x, p.y); });

    py::class_<cvt::Action>(m, "Action")
        .def(py::init<>())
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readonly("unit_ids", &cvt::Action::unit_ids)
        .def_readonly("ability_id", &cvt::Action::ability_id)
        .def_readonly("target_type", &cvt::Action::target_type)
        .def_property_readonly("target_point",
            [](const cvt::Action &action) -> std::optional<cvt::Point2d> {
                return action.target_type == cvt::Action::TargetType::Position ? std::optional{ action.target.point }
                                                                               : std::nullopt;
            })
        .def_property_readonly("target_other",
            [](const cvt::Action &action) -> std::optional<cvt::UID> {
                return action.target_type == cvt::Action::TargetType::OtherUnit ? std::optional{ action.target.other }
                                                                                : std::nullopt;
            })
        .def("__repr__", [](const cvt::Action &a) {
            std::string ret = fmt::format(
                "Action(unit_ids={}, ability_id={}, target_type={}, ", a.unit_ids, a.ability_id, a.target_type);
            if (a.target_type == cvt::Action::TargetType::Position) {
                ret += fmt::format("target_point={})", a.target.point);
            } else {
                ret += fmt::format("target_other={})", a.target.other);
            }
            return ret;
        });

    py::class_<cvt::UnitOrder>(m, "UnitOrder")
        .def_readwrite("ability_id", &cvt::UnitOrder::ability_id)
        .def_readwrite("tgtId", &cvt::UnitOrder::tgtId)
        .def_readwrite("target_pos", &cvt::UnitOrder::target_pos)
        .def_readwrite("progress", &cvt::UnitOrder::progress)
        .def("__repr__", [](const cvt::UnitOrder &x) {
            return fmt::format("UnitOrder(ability_id={}, tgtId={}, target_pos={}, progress={})",
                x.ability_id,
                x.tgtId,
                x.target_pos,
                x.progress);
        });

    py::class_<cvt::Score>(m, "Score")
        .def(py::init<>())
        .def(py::self == py::self)
        .def(py::self != py::self)
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
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const cvt::Unit &data) { return data.id; })
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
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const cvt::NeutralUnit &data) { return data.id; })
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

    py::class_<cvt::StepDataNoUnitsMinimap>(m, "StepDataNoUnitsMinimap")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("gameStep", &cvt::StepDataNoUnitsMinimap::gameStep)
        .def_readwrite("minerals", &cvt::StepDataNoUnitsMinimap::minearals)
        .def_readwrite("vespene", &cvt::StepDataNoUnitsMinimap::vespene)
        .def_readwrite("popMax", &cvt::StepDataNoUnitsMinimap::popMax)
        .def_readwrite("popArmy", &cvt::StepDataNoUnitsMinimap::popArmy)
        .def_readwrite("popWorkers", &cvt::StepDataNoUnitsMinimap::popWorkers)
        .def_readwrite("score", &cvt::StepDataNoUnitsMinimap::score);

    py::class_<cvt::StepDataNoUnitsMinimapSoA>(m, "StepDataNoUnitsMinimapSoA")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("gameStep", &cvt::StepDataNoUnitsMinimapSoA::gameStep)
        .def_readwrite("minerals", &cvt::StepDataNoUnitsMinimapSoA::minearals)
        .def_readwrite("vespene", &cvt::StepDataNoUnitsMinimapSoA::vespene)
        .def_readwrite("popMax", &cvt::StepDataNoUnitsMinimapSoA::popMax)
        .def_readwrite("popArmy", &cvt::StepDataNoUnitsMinimapSoA::popArmy)
        .def_readwrite("popWorkers", &cvt::StepDataNoUnitsMinimapSoA::popWorkers)
        .def_readwrite("score", &cvt::StepDataNoUnitsMinimapSoA::score)
        .def("__len__", &cvt::StepDataNoUnitsMinimapSoA::size)
        .def("__getitem__", [](const cvt::StepDataNoUnitsMinimapSoA &t, std::size_t i) {
            if (i >= t.size()) { throw py::index_error(); }
            return t[i];
        });

    py::class_<cvt::StepDataNoUnits>(m, "StepDataNoUnits")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("gameStep", &cvt::StepDataNoUnits::gameStep)
        .def_readwrite("minerals", &cvt::StepDataNoUnits::minearals)
        .def_readwrite("vespene", &cvt::StepDataNoUnits::vespene)
        .def_readwrite("popMax", &cvt::StepDataNoUnits::popMax)
        .def_readwrite("popArmy", &cvt::StepDataNoUnits::popArmy)
        .def_readwrite("popWorkers", &cvt::StepDataNoUnits::popWorkers)
        .def_readwrite("score", &cvt::StepDataNoUnits::score)
        .def_readwrite("visibility", &cvt::StepDataNoUnits::visibility)
        .def_readwrite("creep", &cvt::StepDataNoUnits::creep)
        .def_readwrite("player_relative", &cvt::StepDataNoUnits::player_relative)
        .def_readwrite("alerts", &cvt::StepDataNoUnits::alerts)
        .def_readwrite("buildable", &cvt::StepDataNoUnits::buildable)
        .def_readwrite("pathable", &cvt::StepDataNoUnits::pathable);

    py::class_<cvt::StepDataNoUnitsSoA>(m, "StepDataNoUnitsSoA")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("gameStep", &cvt::StepDataNoUnitsSoA::gameStep)
        .def_readwrite("minerals", &cvt::StepDataNoUnitsSoA::minearals)
        .def_readwrite("vespene", &cvt::StepDataNoUnitsSoA::vespene)
        .def_readwrite("popMax", &cvt::StepDataNoUnitsSoA::popMax)
        .def_readwrite("popArmy", &cvt::StepDataNoUnitsSoA::popArmy)
        .def_readwrite("popWorkers", &cvt::StepDataNoUnitsSoA::popWorkers)
        .def_readwrite("score", &cvt::StepDataNoUnitsSoA::score)
        .def_readwrite("visibility", &cvt::StepDataNoUnitsSoA::visibility)
        .def_readwrite("creep", &cvt::StepDataNoUnitsSoA::creep)
        .def_readwrite("player_relative", &cvt::StepDataNoUnitsSoA::player_relative)
        .def_readwrite("alerts", &cvt::StepDataNoUnitsSoA::alerts)
        .def_readwrite("buildable", &cvt::StepDataNoUnitsSoA::buildable)
        .def_readwrite("pathable", &cvt::StepDataNoUnitsSoA::pathable)
        .def("__len__", &cvt::StepDataNoUnitsSoA::size)
        .def("__getitem__", [](const cvt::StepDataNoUnitsSoA &t, std::size_t i) {
            if (i >= t.size()) { throw py::index_error(); }
            return t[i];
        });

    py::class_<cvt::StepData>(m, "StepData")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("gameStep", &cvt::StepData::gameStep)
        .def_readwrite("minerals", &cvt::StepData::minearals)
        .def_readwrite("vespene", &cvt::StepData::vespene)
        .def_readwrite("popMax", &cvt::StepData::popMax)
        .def_readwrite("popArmy", &cvt::StepData::popArmy)
        .def_readwrite("popWorkers", &cvt::StepData::popWorkers)
        .def_readwrite("score", &cvt::StepData::score)
        .def_readwrite("visibility", &cvt::StepData::visibility)
        .def_readwrite("creep", &cvt::StepData::creep)
        .def_readwrite("player_relative", &cvt::StepData::player_relative)
        .def_readwrite("alerts", &cvt::StepData::alerts)
        .def_readwrite("buildable", &cvt::StepData::buildable)
        .def_readwrite("pathable", &cvt::StepData::pathable)
        .def_readwrite("actions", &cvt::StepData::actions)
        .def_readwrite("units", &cvt::StepData::units)
        .def_readwrite("neutralUnits", &cvt::StepData::neutralUnits);

    py::class_<cvt::StepDataSoA>(m, "StepDataSoA")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("gameStep", &cvt::StepDataSoA::gameStep)
        .def_readwrite("minerals", &cvt::StepDataSoA::minearals)
        .def_readwrite("vespene", &cvt::StepDataSoA::vespene)
        .def_readwrite("popMax", &cvt::StepDataSoA::popMax)
        .def_readwrite("popArmy", &cvt::StepDataSoA::popArmy)
        .def_readwrite("popWorkers", &cvt::StepDataSoA::popWorkers)
        .def_readwrite("score", &cvt::StepDataSoA::score)
        .def_readwrite("visibility", &cvt::StepDataSoA::visibility)
        .def_readwrite("creep", &cvt::StepDataSoA::creep)
        .def_readwrite("player_relative", &cvt::StepDataSoA::player_relative)
        .def_readwrite("alerts", &cvt::StepDataSoA::alerts)
        .def_readwrite("buildable", &cvt::StepDataSoA::buildable)
        .def_readwrite("pathable", &cvt::StepDataSoA::pathable)
        .def_readwrite("actions", &cvt::StepDataSoA::actions)
        .def_readwrite("units", &cvt::StepDataSoA::units)
        .def_readwrite("neutralUnits", &cvt::StepDataSoA::neutralUnits)
        .def("__len__", &cvt::StepDataSoA::size)
        .def("__getitem__", [](const cvt::StepDataSoA &t, std::size_t i) {
            if (i >= t.size()) { throw py::index_error(); }
            return t[i];
        });

    py::class_<cvt::ReplayInfo>(m, "ReplayInfo")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("replayHash", &cvt::ReplayInfo::replayHash)
        .def_readwrite("gameVersion", &cvt::ReplayInfo::gameVersion)
        .def_readwrite("playerId", &cvt::ReplayInfo::playerId)
        .def_readwrite("durationSteps", &cvt::ReplayInfo::durationSteps)
        .def_readwrite("playerRace", &cvt::ReplayInfo::playerRace)
        .def_readwrite("playerResult", &cvt::ReplayInfo::playerResult)
        .def_readwrite("playerMMR", &cvt::ReplayInfo::playerMMR)
        .def_readwrite("playerAPM", &cvt::ReplayInfo::playerAPM)
        .def_readwrite("mapWidth", &cvt::ReplayInfo::mapWidth)
        .def_readwrite("mapHeight", &cvt::ReplayInfo::mapHeight)
        .def_readwrite("heightMap", &cvt::ReplayInfo::heightMap)
        .def("__repr__", [](const cvt::ReplayInfo &info) {
            return fmt::format(
                "ReplayInfo(replayHash={}, gameVersion={}, playerId={}, durationSteps={}, playerRace={}, "
                "playerResult={}, playerMMR={}, playerAPM={}, mapWidth={}, mapHeight={}, heightMap=array([]))",
                info.replayHash,
                info.gameVersion,
                info.playerId,
                info.durationSteps,
                info.playerRace,
                info.playerResult,
                info.playerMMR,
                info.playerAPM,
                info.mapWidth,
                info.mapHeight);
        });

    bindReplayDataInterfaces<cvt::ReplayDataSoANoUnitsMiniMap>(m, "ReplayDataScalarOnly");

    bindReplayDataInterfaces<cvt::ReplayDataSoANoUnits>(m, "ReplayDataNoUnits");

    bindReplayDataInterfaces<cvt::ReplayDataSoA>(m, "ReplayDataAll");

    m.def("set_replay_database_logger_level", &cvt::setReplayDBLoggingLevel, py::arg("lvl"));
}
