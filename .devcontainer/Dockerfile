# Use a Debian testing image for access to newer GCC/G++ versions
FROM debian:testing

ENV DEBIAN_FRONTEND=noninteractive

# Install core build tools, SSL certs, GCC/G++ 14, and libraries needed for Crow
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
    docker.io \
    gcc-14 \
    g++-14 && \
    update-ca-certificates && \
    # Set GCC/G++ 14 as the default
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# --- Install Crow Library ---
RUN mkdir -p /tmp/crow_build && \
    cd /tmp/crow_build && \
    git clone --recursive https://github.com/CrowCpp/Crow.git . && \
    mkdir build && \
    cd build && \
    cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF -DCROW_ENABLE_SSL=OFF CROW_ENABLE_COMPRESSION=OFF && \
    cmake --build . && \
    cmake --build . --target install && \
    cd / && rm -rf /tmp/crow_build

# Create non-root 'vscode' user
RUN useradd -m vscode && \
    apt-get update && apt-get install -y sudo && \
    echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
    rm -rf /var/lib/apt/lists/*

USER vscode
WORKDIR /home/vscode/app
