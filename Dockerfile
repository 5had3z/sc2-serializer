# Download and compile zlib-ng
FROM ubuntu:22.04 AS zlib-ng-builder

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    wget tar build-essential ninja-build cmake

WORKDIR /opt/zlib-ng
RUN wget https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.1.5.tar.gz && \
    tar -xf 2.1.5.tar.gz && \
    rm 2.1.5.tar.gz &&\
    cmake -B build -S zlib-ng-2.1.5 -G Ninja -DZLIB_COMPAT=ON -DZLIB_ENABLE_TESTS=OFF -DWITH_NATIVE_INSTRUCTIONS=ON && \
    cmake --build build --parallel

# Compile Converter and Merger Programs
FROM ubuntu:22.04 AS builder

# Add GCC 13
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y gcc-13 g++-13

# Add other build deps
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cmake ninja-build git libboost-iostreams1.74-dev python3-dev libtbb-dev

WORKDIR /app
COPY . .
RUN CC=/usr/bin/gcc-13 CXX=/usr/bin/g++-13 \
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSC2_PY_READER=OFF -DSC2_TESTS=OFF -DBUILD_API_EXAMPLES=OFF && \
    cmake --build build --parallel --config Release

# Runner Stage
FROM ubuntu:22.04 AS runner

# Add Newer libstdc++6 and iostreams
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libboost-iostreams1.74.0 libstdc++6 python3-dev python3-pip libtbb12 && pip3 install mpyq

COPY --from=zlib-ng-builder /opt/zlib-ng/build/libz.so.1.3.0.zlib-ng /opt/zlib-ng/libz.so.1.3.0.zlib-ng
ENV LD_PRELOAD=/opt/zlib-ng/libz.so.1.3.0.zlib-ng

WORKDIR /app
COPY --from=builder \
    /app/build/sc2_converter \
    /app/build/sc2_merger \
    /app/build/format_converter \
    /app/

ENTRYPOINT [ "./sc2_converter" ]
CMD [ "-h" ]
