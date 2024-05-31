.. _scripts:

Scripts
=======

Various helper scripts are included in the complete source code but not included in the python library, since they mostly deal with the dataset generation, rather than dataloading.

find_all_versions
-----------------

.. autofunction:: find_all_versions.compare_replays_and_game

.. autofunction:: find_all_versions.write_replay_versions


gen_info_header
---------------

Generates generated_info.hpp


gen_info_yaml
-------------

Generates game_info.yaml


inspect_replay
--------------

.. autofunction:: inspect_replay.inspect

.. autofunction:: inspect_replay.count


make_partitions
---------------

.. autofunction:: make_partitions.main


merge_info_yaml
---------------

.. autofunction:: merge_info_yaml.main


replay_parallel
---------------

.. autofunction:: replay_parallel.main


replay_sql
----------

.. autofunction:: replay_sql.create

.. autofunction:: replay_sql.create_individual

.. autofunction:: replay_sql.merge


review_resources
----------------

.. autofunction:: review_resources.main
