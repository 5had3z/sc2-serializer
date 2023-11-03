#include <pybind11/pybind11.h>

int add(int i, int j) { return i + j; }

PYBIND11_MODULE(replay_reader, m)
{
    m.doc() = "pbdoc(Python bindings for Starcraft II database reader)pbdoc";
    m.def("add", &add, "pbdoc(Add two numbers test)pbdoc");
    m.attr("__version__") = "0.0.1";
}