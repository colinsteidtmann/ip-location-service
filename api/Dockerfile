FROM debian:testing

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    net-tools \
    cmake \
    build-essential \
    curl \
    wget \
    git \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-program-options-dev \
    libssl-dev \
    libjsoncpp-dev \
    libpq-dev \
    libpqxx-dev \
    libasio-dev \
    libhiredis-dev \
    gcc-14 \
    g++-14 \
    sudo && \
    update-ca-certificates && \
    #set GCC/G++ 14 as the default
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

#redis++
RUN mkdir -p /tmp/redispp_build && \
    cd /tmp/redispp_build && \
    git clone https://github.com/sewenew/redis-plus-plus.git . && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DREDIS_PLUS_PLUS_CXX_STANDARD=20 && \
    make && \
    make install && \
    ldconfig && \
    cd / && rm -rf /tmp/redispp_build

#crow
RUN mkdir -p /tmp/crow_build && \
    cd /tmp/crow_build && \
    git clone --recursive https://github.com/CrowCpp/Crow.git . && \
    mkdir build && \
    cd build && \
    cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF -DCROW_ENABLE_SSL=OFF -DCROW_ENABLE_COMPRESSION=OFF && \
    cmake --build . && \
    cmake --build . --target install && \
    cd / && rm -rf /tmp/crow_build

#create non-root 'appuser' user with /bin/bash shell and add to sudoers
RUN useradd -ms /bin/bash appuser && \
    echo "appuser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER appuser
WORKDIR /home/appuser/app