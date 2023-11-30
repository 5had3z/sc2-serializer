# Compilation Stage
FROM ubuntu:22.04 AS builder

# Add GCC 13
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y gcc-13 g++-13

# Add other build deps
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cmake ninja-build git libboost-iostreams1.74-dev

WORKDIR /app
COPY . .
RUN CC=/usr/bin/gcc-13 CXX=/usr/bin/g++-13 \
    cmake -B build -G Ninja -DSC2_PY_READER=OFF -DSC2_TESTS=OFF -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel --config Release

# Runner Stage
FROM ubuntu:22.04 AS runner

# Add Newer libstdc++6 and iostreams
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y libboost-iostreams1.74.0 libstdc++6

COPY --from=builder /app/build/sc2_converter /sc2_converter
COPY --from=builder /app/build/sc2_merger /sc2_merger


ENTRYPOINT [ "/app/sc2_converter" ]
CMD [ "-h" ]
