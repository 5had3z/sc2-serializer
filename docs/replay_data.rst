.. _api_data:

Replay Data
===========

Pre-Converted Tournament Data
-----------------------------

Pre-serialized tournament data is available to `download <https://bridges.monash.edu/articles/dataset/Tournament_Starcraft_II/25865566>`_. Each replay database file is named after the tournament it was gathered from. The associated SQLite database (gamedata.db) which contains metadata on each of the replays is also included in this pack. This data was gathered with the ``Action`` converter, i.e. replay observations are only recorded on player actions, which is the same method as `AlphaStar-Unplugged <https://github.com/google-deepmind/alphastar/blob/main/alphastar/unplugged/data/README.md>`_. Another tournament dataset created (and not currently posted online) used ``Strided+Action`` with a stride of ~1sec (IIRC?). This increased the overall size of the dataset to ~90GB, rather than ``Action`` only ~55.5GB.


Downloading Replay Data 
-----------------------

Blizzard Replay Packs
^^^^^^^^^^^^^^^^^^^^^

Blizzard have replay packs available grouped by game version played on. These replay packs can be downloaded using their `download_replays.py` script which can be found `here <https://github.com/Blizzard/s2client-proto/tree/master/samples/replay-api>`_. The game version associated with each replay pack to actually run the replays can also be downloaded from `here <https://github.com/Blizzard/s2client-proto#downloads>`_.

Tournament Replay Packs 
^^^^^^^^^^^^^^^^^^^^^^^

Tournament replay packs are gathered and provided by a `community website <https://lotv.spawningtool.com/replaypacks>`_ which is regularly updated. Unfortunately Blizzard have not released headless linux versions of StarCraft II since 4.10 (at the time of writing). The newer tournament replays must be played with the windows client. ``sc2-serializer`` is compatible with being compiled and run on windows. We include a script to launch many copies of StarCraftII using a unique port for communication to enable processing of many tournament replays in parallel on a windows machine.

A particular problem with tournament replays is that many of the game sub-versions and maps are unique and need to be downloaded. Blizzard's CLI replay client is unable to download this data automatically, hence each replay that doesn't work due to missing data needs to be individually opened by a user with the normal game client to initiate the download process. I assuming posting the client data is prohibited by some non-distribution eula, or else I would post this to save someone else many hours of this dull task. One key tip to check if data is missing and has to be downloaded is that the minimap preview in-game is displayed when the data is accounted for, and not present when missing. So skip over games if the minimap preview is there, and manually open games when it is not. After the game begins, exit the game and repeat. Then the conversion process can be run, mostly uninterrupted. There are some games when the client freezes at the same point in the replay. This usually cannot be fixed by restarting the replay, the replay is just not convertable for unknown reasons.


Converting Replays 
------------------

Once you have acquired replays to serialize, ``sc2_converter`` is used to re-run the replays and record the observations to a new ``.SC2Replays`` file. ``sc2_converter`` includes a ``-h/--help`` flag to print out all the current options available for the conversion process. An example of running this program is below.

.. code::

    ./sc2_converter --replays=/folder/of/replays --output=/path/output.SC2Replays --game=/SC2/492/Versions --converter=strided --save-actions --stride=22

Several instances of this program can be run in parallel. Each instance needs a unique ``--port`` for communication between the client (observer) and server (SC2), unless they're running in isolated docker containers. The database interface isn't inter-process safe or anything, hence each instance should be writing to their own output file. If you want to merge the results together, ``sc2_merger`` can be used.

Originally there was some poor choices in the ``.SC2Replays`` format and structure, so other programs like ``fix_lut`` and ``format_converter`` were used to fix this (instead of running conversion from scratch). These aren't really used at this point, and are mostly historical, maybe used as a foundation for new things.
