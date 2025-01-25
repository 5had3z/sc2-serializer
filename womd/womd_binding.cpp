/**
 * @file womd_binding.cpp
 * @author Bryce Ferenczi (frenzi@hotmai.com.au)
 * @brief Python Bindings to WOMD dataset
 * @version 0.1
 * @date 2025-01-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <cstdint>
#include <string>
#include <vector>

#include <database.hpp>
#include <instance_transform.hpp>

#include <boost/pfr.hpp>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;
namespace pfr = boost::pfr;

using mask_t = std::uint8_t;

struct Agent
{
    bool is_sdc;
    float id;
    float type;
    float bbox_yaw;
    float height;
    float length;
    float width;
    float vel_yaw;
    float velocity_x;
    float velocity_y;
    float x;
    float y;
    float z;
    std::int64_t tracks_to_predict;
    std::int64_t timestamp_micros;
};

struct AgentSoA
{
    using struct_type = Agent;

    std::vector<char> is_sdc;
    std::vector<float> id;
    std::vector<float> type;
    std::vector<float> bbox_yaw;
    std::vector<float> height;
    std::vector<float> length;
    std::vector<float> width;
    std::vector<float> vel_yaw;
    std::vector<float> velocity_x;
    std::vector<float> velocity_y;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    std::vector<std::int64_t> tracks_to_predict;
    std::vector<std::int64_t> timestamp_micros;

    [[nodiscard]] auto size() const noexcept -> std::size_t { return id.size(); }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> struct_type
    {
        return cvt::gatherStructAtIndex(*this, index);
    }
};

struct TrafficLight
{
    float x;
    float y;
    float z;
    std::int32_t state;
    std::int64_t id;
    std::int64_t timestamp_micros;
};

struct TrafficLightSoA
{
    using struct_type = TrafficLight;

    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    std::vector<std::int32_t> state;
    std::vector<std::int64_t> id;
    std::vector<std::int64_t> timestamp_micros;

    [[nodiscard]] auto size() const noexcept -> std::size_t { return id.size(); }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> struct_type
    {
        return cvt::gatherStructAtIndex(*this, index);
    }
};

struct RoadGraph
{
    std::vector<std::int64_t> id;
    std::vector<std::int64_t> type;
    std::vector<float> dir;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
};


struct SequenceData
{
    std::string scenarioId;
    RoadGraph roadGraph;
    std::vector<std::vector<Agent>> agentData;
    std::vector<std::vector<TrafficLight>> signalsData;
};

template<> struct cvt::DatabaseInterface<SequenceData>
{
    using header_type = std::string;

    static auto getHeaderImpl(std::istream &dbStream) -> std::string
    {
        std::string header;
        cvt::deserialize(header, dbStream);
        return header;
    }

    // Our "uid" is the same as the header for this simple example
    static auto getEntryUIDImpl(std::istream &dbStream) -> std::string { return getHeaderImpl(dbStream); }

    static auto getEntryImpl(std::istream &dbStream) -> SequenceData
    {
        SequenceData result;
        cvt::deserialize(result.scenarioId, dbStream);
        cvt::deserialize(result.roadGraph, dbStream);
        {
            cvt::FlattenedData2<AgentSoA> agents;
            cvt::deserialize(agents, dbStream);
            result.agentData = cvt::recoverFlattenedSortedData2(agents);
        }
        {
            cvt::FlattenedData2<TrafficLightSoA> signals;
            cvt::deserialize(signals, dbStream);
            result.signalsData = cvt::recoverFlattenedSortedData2(signals);
        }
        return result;
    }

    static auto addEntryImpl(const SequenceData &d, std::ostream &dbStream) noexcept -> bool
    {
        cvt::serialize(d.scenarioId, dbStream);
        cvt::serialize(d.roadGraph, dbStream);
        // Both agents and traffic lights have same 'id' property we want to sort by, so we can use the same lambda
        auto cmpFn = [](auto &&a, auto &&b) { return a.second.id < b.second.id; };
        cvt::serialize(cvt::flattenAndSortData2<AgentSoA>(d.agentData, cmpFn), dbStream);
        cvt::serialize(cvt::flattenAndSortData2<TrafficLightSoA>(d.signalsData, cmpFn), dbStream);
        return true;
    }
};

using WomdDatabase = cvt::ReplayDatabase<SequenceData>;

template<typename T>
auto toVectorOfVectors(const py::array_t<float> &array, const py::array_t<mask_t> &mask) -> std::vector<std::vector<T>>
{
    std::vector<std::vector<T>> result{};
    if (array.shape(2) != pfr::tuple_size_v<T>) { throw std::runtime_error("Invalid number of fields in the array"); }

    auto unchecked = array.unchecked<3>();
    auto mask_unchecked = mask.unchecked<2>();
    for (py::ssize_t i = 0; i < unchecked.shape(0); ++i) {
        auto inner = result.emplace_back();
        for (py::ssize_t j = 0; j < unchecked.shape(1); ++j) {
            if (mask_unchecked(i, j) == 0) { continue; }
            py::ssize_t k = 0;
            pfr::for_each_field(inner.emplace_back(), [&](auto &field) { field = unchecked(i, j, k++); });
        }
    }
    return result;
}

auto parseSequenceFromArray(const py::array_t<float> &agents,
    const py::array_t<mask_t> &agents_mask,
    const py::array_t<float> &traffic,
    const py::array_t<mask_t> &traffic_mask,
    const py::array_t<float> &roadgraph,
    const py::array_t<mask_t> &roadgraph_mask,
    const std::string &scenarioId) -> SequenceData
{
    SequenceData result;
    result.scenarioId = scenarioId;
    result.agentData = toVectorOfVectors<Agent>(agents, agents_mask);
    result.signalsData = toVectorOfVectors<TrafficLight>(traffic, traffic_mask);

    // Parse road graph
    {
        auto &road = result.roadGraph;
        auto roadgraph_unchecked = roadgraph.unchecked<2>();
        for (py::ssize_t i = 0; i < roadgraph_unchecked.shape(0); ++i) {
            road.id.emplace_back(roadgraph_unchecked(i, 0));
            road.type.emplace_back(roadgraph_unchecked(i, 1));
            road.dir.emplace_back(roadgraph_unchecked(i, 2));
            road.x.emplace_back(roadgraph_unchecked(i, 3));
            road.y.emplace_back(roadgraph_unchecked(i, 4));
            road.z.emplace_back(roadgraph_unchecked(i, 5));
        }
    }

    return result;
}


PYBIND11_MODULE(_womd_binding, m)
{
    m.doc() = "pbdoc(Python bindings for Waymo Open Motion Data with Serializer)pbdoc";
    m.attr("__version__") = "0.0.1";

    py::class_<Agent>(m, "Agent")
        .def(py::init<>())
        .def_readwrite("is_sdc", &Agent::is_sdc)
        .def_readwrite("id", &Agent::id)
        .def_readwrite("type", &Agent::type)
        .def_readwrite("bbox_yaw", &Agent::bbox_yaw)
        .def_readwrite("height", &Agent::height)
        .def_readwrite("length", &Agent::length)
        .def_readwrite("width", &Agent::width)
        .def_readwrite("vel_yaw", &Agent::vel_yaw)
        .def_readwrite("velocity_x", &Agent::velocity_x)
        .def_readwrite("velocity_y", &Agent::velocity_y)
        .def_readwrite("x", &Agent::x)
        .def_readwrite("y", &Agent::y)
        .def_readwrite("z", &Agent::z)
        .def_readwrite("tracks_to_predict", &Agent::tracks_to_predict)
        .def_readwrite("timestamp_micros", &Agent::timestamp_micros);

    py::class_<TrafficLight>(m, "TrafficLight")
        .def(py::init<>())
        .def_readwrite("x", &TrafficLight::x)
        .def_readwrite("y", &TrafficLight::y)
        .def_readwrite("z", &TrafficLight::z)
        .def_readwrite("state", &TrafficLight::state);

    py::class_<RoadGraph>(m, "RoadGraph")
        .def(py::init<>())
        .def_readwrite("id", &RoadGraph::id)
        .def_readwrite("type", &RoadGraph::type)
        .def_readwrite("dir", &RoadGraph::dir)
        .def_readwrite("x", &RoadGraph::x)
        .def_readwrite("y", &RoadGraph::y)
        .def_readwrite("z", &RoadGraph::z);


    py::class_<SequenceData>(m, "SequenceData")
        .def(py::init<>())
        .def_readwrite("scenarioId", &SequenceData::scenarioId)
        .def_readwrite("roadGraph", &SequenceData::roadGraph)
        .def_readwrite("agentData", &SequenceData::agentData)
        .def_readwrite("signalsData", &SequenceData::signalsData);

    py::class_<WomdDatabase>(m, "WomdDatabase")
        .def(py::init<>())
        .def(py::init<const std::filesystem::path &>(), py::arg("dbPath"))
        .def("open", &WomdDatabase::open, py::arg("dbPath"))
        .def("create", &WomdDatabase::create, py::arg("dbPath"))
        .def("load", &WomdDatabase::load, py::arg("dbPath"))
        .def("isFull", &WomdDatabase::isFull)
        .def("size", &WomdDatabase::size)
        .def("__len__", &WomdDatabase::size)
        .def("addEntry", &WomdDatabase::addEntry, py::arg("data"))
        .def("getEntry", &WomdDatabase::getEntry, py::arg("index"))
        .def("__getitem__", &WomdDatabase::getEntry, py::arg("index"))
        .def("getHeader", &WomdDatabase::getHeader, py::arg("index"))
        .def("getEntryUID", &WomdDatabase::getEntryUID, py::arg("index"))
        .def_property_readonly("path", &WomdDatabase::path);

    m.def("parseSequenceFromArray",
        &parseSequenceFromArray,
        py::arg("agents"),
        py::arg("agents_mask"),
        py::arg("traffic"),
        py::arg("traffic_mask"),
        py::arg("roadgraph"),
        py::arg("roadgraph_mask"),
        py::arg("scenarioId"));
}
