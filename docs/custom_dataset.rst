.. _custom_dataset:

Applying to New Dataset
=======================

Much of the code has been written with templates so there are plenty of customization points where you can make simple modifications to fit your needs.

We include an example of a custom project in ``bin/custom_example.cpp``. This example demonstrates a scenario where of a set of humans and robots observed in an environment over time, each with varying and random properties. You can run this example ``./example_custom --file test.db --duration 10`` and see some data printed as the data is generated and written to the database. This entry can be then read-back to the user with ``./example_custom --file test.db --index 0`` and it should print back the same metadata logs that occurred during generation/writing.


Serialized Database
-------------------

``ReplayDatabase`` is templated on a which specializes the ``DatabaseInterface`` type, hence fulfils the ``HasDBInterface`` concept. This ``DatabaseInterface`` defines how to read and write that type to the database, as well as other utilities such as only reading the header to get ``ReplayInfo`` metadata. The metadata/header is relatively hard-coded into the ``DatabaseInterface``, it is up to the user to modify that as appropriate.

.. code-block:: c++

    #include "replay_interface.hpp"

    namespace cvt {

    struct EntryMeta {
        std::string location;
        float some_property;
    };

    struct MyDatasetEntry {
        using header_type = EntryMeta;

        EntryMeta header;
        std::vector<float> timeseriesA;
        std::vector<float> timeseriesB;
    };

    template<> struct DatabaseInterface<MyDatasetEntry> {

        static auto getHeaderImpl(std::istream &dbStream) -> EntryMeta
        {
            EntryMeta result;
            deserialize(result, dbStream);
            return result;
        }

        static auto getEntryImpl(std::istream &dbStream) -> MyDatasetEntry
        {
            MyDatasetEntry data;
            deserialize(data, dbStream);
            return data;
        }

        // Implement other methods

    };

    using CustomDatabase = ReplayDatabase<MyDatasetEntry>;

    } // namespace cvt


Automatic Struct to Vector and Enums
------------------------------------

This process is automated for most common datatypes (struct, vector, enum, numeric) and is applied recursively. Enums that need the auto one-hot encoding transform must be added to the chain of ``if constexpr`` in ``getEnumValues()`` in ``enums.hpp``. Then any struct that contains the enum can be vectorized with a one-hot encoding of that enum. Mapping types haven't been implemented as there hasn't been a use case for it yet.


Struct-of-Array <-> Array-of-Struct
-----------------------------------

A **Struct-of-Arrays** representation of the target *Struct* that fulfils the ``IsSoAType`` concept must be defined with identical names between the members for the automatic ``AoStoSoA`` and ``SoAtoAoS`` to function correctly. ``cvt::gatherStructAtIndex`` can be used when defining ``operator[]`` in the SoA type to automate gathering data from each of the vectors in the *SoA*.

A simple example of a struct ``A`` and its SoA is shown below. This method only applies top level struct, and is not recursive, hence ``ASoA`` contains ``std::vector<B>``. The field names can be out-of-order between ``A`` and ``ASoA``. At compile-time fields are matched by name rather than by index. Hence, there must be a 1-to-1 mapping between field (you should get a compile-time error otherwise). Note, there is no check for identical types between names, only that one is assignable to another.

.. code-block:: c++

    struct B {
        int a;
        int b;
    };

    struct A
    {
        float a;
        int b;
        B c;
    };

    struct ASoA
    {
        using struct_type = A;
        std::vector<float> a;
        std::vector<int> b;
        std::vector<B> c;

        auto operator[](std::size_t index) const noexcept -> struct_type {
            return cvt::gatherStructAtIndex(*this, index);
        }

        auto size() const noexcept -> std::size_t { return a.size(); }
    };

Specialized **SoA<->AoS** transforms can be defined if the automated process isn't appropriate. For example ``ReplayData`` and ``ReplayDataSoA`` is a struct containing a header and the replay data. Hence, we plainly copy the Header data, and perform the transform on the replay observation data (see ``aos_impl.cpp``).


Instance Transform (flattenAndSortData(2))
------------------------------------------

The transform that converts the time-major representation of the units to an instance-major **SoA** is ``flattenAndSortData`` and recovers back to the time-major is ``recoverFlattenedSortedData``. The **v2** of these functions (suffixed by 2), further compresses the time indices as [start,count] pairs. This didn't have a significant impact to final filesize, but the work was done for a more sophisticated algorithm so it might as well be used.

These functions accept any ``IsSoAType`` and use a user-defined ``Comp`` function which enables sorting of any underlying *struct* by any field. This comparison function applies the comparison on a StepIndex and Struct pair, hence using ``.second`` is needed to access your struct. The code for sorting the SC2 Unit observation data is shown below. The recovery will not return the same order of units in each timestep (the inner std::vector), but they will still be the same set of units in each timestep.

.. code-block:: c++

    auto getInstanceSortedUnits(const std::vector<std::vector<Unit>>& units)
    {
        // Full type in lambda for verbosity, but you are encouraged to use auto...
        auto byInstanceId = [](const std::pair<std::uint32_t, T>& a, const std::pair<std::uint32_t, T>& b) {
            return a.second.id < b.second.id;
        };
        return cvt::flattenAndSortData<cvt::UnitSoA>(replayData.data.units, byInstanceId);
    }
