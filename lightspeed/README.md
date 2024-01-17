# Lightspeed Benchmark

## Summary

This is a small experiment to see the maximum possible speed for processing replays by only sending step requests and receiving observation messages.

## Results

sc2-serializer using the sc2-api interface is near parity to lightspeed, even when running 16 in parallel on a 5950X machine with 64GB Memory. Hence, it is not worth it from a performance point of view to refactor the code for using the raw protobuf. Alphastar is very close to running with 16 threads on a 64GB machine for the chosen replay. However, there are much larger replays which are deep into OOM territory.

Benchmarked with 00004e179daa5a5bafeef22c01bc84408f70052a7e056df5c63800aed85099e9 from 4.9.2 replay pack.

| Program        | 1-Thread | 8-Thread | 16-Thread | 24-Thread |
|----------------|----------|----------|-----------|-----------|
| lightspeed     | 90       | 124      | ~200      | ~300      |
| sc2-serializer | 90       | 131      | ~200      | ~300      |
| alphastar      | 131      | 150      | OOM       | OOM       |
