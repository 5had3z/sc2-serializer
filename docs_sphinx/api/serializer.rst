.. _api_serialize:

Serialization
=============

A combination of C++20 Concepts and Requires clauses are used to automatically serialize data structures. [Boost PFR](https://www.boost.org/doc/libs/master/doc/html/boost_pfr.html) is used to iterate over data structures. Data structures must be trivially copyable or some range which can be copied element-by-element.

.. doxygenfile:: serialize.hpp
   :project: StarCraft II Serializer
