# Lightspeed Benchmark

## Summary

This is a small experiment to see the maximum possible speed for processing replays by only sending step requests and receiving observation messages.

## Results

sc2-serializer using the sc2-api interface is near parity to lightspeed, even when running 16 in parallel on a 5950X machine. Hence, it is not worth it from a performance point of view to refactor the code for using the raw protobuf.

Benchmarked with 00004e179daa5a5bafeef22c01bc84408f70052a7e056df5c63800aed85099e9 from 4.9.2 replay pack. Takes around 90 seconds for a single thread or 200 seconds when running 16 independent threads.
