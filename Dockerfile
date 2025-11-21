# Multi-stage Dockerfile for C++ crawler applications
# Supports both ARM64 and AMD64 architectures

FROM debian:bookworm-slim AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libcurl4-openssl-dev \
    libssl-dev \
    nlohmann-json3-dev \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg for C++ package management
WORKDIR /opt
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    cd vcpkg && \
    ./bootstrap-vcpkg.sh && \
    ./vcpkg integrate install

# Install google-cloud-cpp via vcpkg
RUN /opt/vcpkg/vcpkg install google-cloud-cpp[pubsub] curl

# Set up the build environment
WORKDIR /app
COPY . .

# Create CMakeLists.txt for building the applications
RUN cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.15)
project(Crawler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(google_cloud_cpp_pubsub REQUIRED)

# Library
add_library(CrawlerLib STATIC
    Library/Library.cpp
    Library/config.cpp
    Library/pch.cpp
)

target_include_directories(CrawlerLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Library
)

target_link_libraries(CrawlerLib PUBLIC
    CURL::libcurl
    google-cloud-cpp::pubsub
)

# LinkFinder
add_executable(LinkFinder LinkFinder/LinkFinder.cpp)
target_link_libraries(LinkFinder PRIVATE CrawlerLib)

# ProfileFinder
add_executable(ProfileFinder ProfileFinder/ProfileFinder.cpp)
target_link_libraries(ProfileFinder PRIVATE CrawlerLib)

# HTMLCrawler
add_executable(HTMLCrawler HTMLCrawler/HTMLCrawler.cpp)
target_link_libraries(HTMLCrawler PRIVATE CrawlerLib)
EOF

# Build the applications
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --parallel $(nproc)

# Runtime stage
FROM debian:bookworm-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy built binaries
WORKDIR /app
COPY --from=builder /app/build/LinkFinder /app/LinkFinder
COPY --from=builder /app/build/ProfileFinder /app/ProfileFinder
COPY --from=builder /app/build/HTMLCrawler /app/HTMLCrawler

# Default command (can be overridden)
CMD ["/app/LinkFinder"]
