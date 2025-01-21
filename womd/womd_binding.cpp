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
#include <pybind11/pybind11.h>

namespace py = pybind11;

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


struct SequenceInfo
{
    std::string scenario_id;
    RoadGraph roadGraph;
};

struct SequenceData
{
    SequenceInfo header;
    std::vector<std::vector<Agent>> agent_data;
    std::vector<std::vector<TrafficLight>> signals_data;
};

template<> struct cvt::DatabaseInterface<SequenceData>
{
    using header_type = std::string;

    static auto getHeaderImpl(std::istream &dbStream) -> SequenceInfo
    {
        SequenceInfo header;
        cvt::deserialize(header, dbStream);
        return header;
    }

    // Our "uid" is the same as the header for this simple example
    static auto getEntryUIDImpl(std::istream &dbStream) -> std::string { return getHeaderImpl(dbStream).scenario_id; }

    static auto getEntryImpl(std::istream &dbStream) -> SequenceData
    {
        SequenceData result;
        cvt::deserialize(result.header, dbStream);
        {
            cvt::FlattenedData2<AgentSoA> agents;
            cvt::deserialize(agents, dbStream);
            result.agent_data = cvt::recoverFlattenedSortedData2(agents);
        }
        {
            cvt::FlattenedData2<TrafficLightSoA> signals;
            cvt::deserialize(signals, dbStream);
            result.signals_data = cvt::recoverFlattenedSortedData2(signals);
        }
        return result;
    }

    static auto addEntryImpl(const SequenceData &d, std::ostream &dbStream) noexcept -> bool
    {
        cvt::serialize(d.header, dbStream);
        // Both agents and traffic lights have same 'id' property we want to sort by, so we can use the same lambda
        auto cmpFn = [](auto &&a, auto &&b) { return a.second.id < b.second.id; };
        cvt::serialize(cvt::flattenAndSortData2<AgentSoA>(d.agent_data, cmpFn), dbStream);
        cvt::serialize(cvt::flattenAndSortData2<TrafficLightSoA>(d.signals_data, cmpFn), dbStream);
        return true;
    }
};

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
}
