.. _api_data:

Starcraft II Data Structures
============================

Common
------

Includes common tools/structures as well as the score and action struct.

.. doxygenfile:: data_structures/common.hpp
   :project: C++ Sphinx Doxygen Breathe


Enums
-----

Includes most of the enums used in SC2 (Outcome, Alliance, etc.)

.. doxygenfile:: data_structures/enums.hpp
   :project: C++ Sphinx Doxygen Breathe


Units
-----

Unit and neutral unit data structures with their AoS<->SoA transforms.

.. doxygenfile:: data_structures/units.hpp
   :project: C++ Sphinx Doxygen Breathe


Interface
---------

Interface between replay structure and database

.. doxygenfile:: data_structures/replay_interface.hpp
   :project: C++ Sphinx Doxygen Breathe


All Data
--------

All-data replay structure (units, minimaps, scalars).

.. doxygenfile:: data_structures/replay_all.hpp
   :project: C++ Sphinx Doxygen Breathe


Minimap + Scalar Data
---------------------

Unit data is omitted, only minimap and scalar data is in here.

.. doxygenfile:: data_structures/replay_minimaps.hpp
   :project: C++ Sphinx Doxygen Breathe


Scalar Data
-----------

Only scalar data such as economics and score are here.

.. doxygenfile:: data_structures/replay_scalars.hpp
   :project: C++ Sphinx Doxygen Breathe
