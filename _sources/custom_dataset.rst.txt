.. _custom_dataset:

Applying to New Dataset
=======================

Much of the code has been written with templates such that there are plenty of customization points where you can make modifications to fit your needs. For example, ReplayDatabase is templated on some type, which specializes the DatabaseInterface type, hence fulfils the HasDBInterface concept. This DatabaseInterface defines how to read and write that type to the database, as well as other utilities such as only reading the header to get ReplayInfo metadata.
