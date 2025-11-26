ARG BUILDPLATFORM
ARG TARGETPLATFORM
ARG TARGETARCH
ARG TARGETOS

# Builder stage
FROM --platform=${TARGETPLATFORM} debian:bookworm-slim AS builder

ARG TARGETARCH

# Install build dependencies
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       build-essential \
       ca-certificates \
       cmake \
       git \
       curl \
       ninja-build \
       pkg-config \
       zip \
       unzip \
       tar \
    && rm -rf /var/lib/apt/lists/*

# Set vcpkg triplet based on target architecture
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_BINARY_SOURCES="clear;default" \
    VCPKG_FEATURE_FLAGS=manifests \
    VCPKG_BUILD_TYPE=release

RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# Install required libraries with correct triplet
RUN TRIPLET=$(if [ "$TARGETARCH" = "arm64" ]; then echo "arm64-linux"; else echo "x64-linux"; fi) && \
    VCPKG_BUILD_TYPE=release ${VCPKG_ROOT}/vcpkg install google-cloud-cpp[pubsub]:${TRIPLET} curl:${TRIPLET} --clean-after-build

WORKDIR /src

COPY . .

# Configure and build with vcpkg toolchain
RUN TRIPLET=$(if [ "$TARGETARCH" = "arm64" ]; then echo "arm64-linux"; else echo "x64-linux"; fi) && \
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=${TRIPLET} \
    -DVCPKG_LIBRARY_LINKAGE=static \
    -DVCPKG_BUILD_TYPE=release \
    && cmake --build build --parallel

# Collect binaries
RUN mkdir -p /opt/crawler/bin \
    && cp build/HTMLCrawler/HTMLCrawler /opt/crawler/bin/ \
    && cp build/LinkFinder/LinkFinder /opt/crawler/bin/ \
    && cp build/ProfileFinder/ProfileFinder /opt/crawler/bin/ \
    && cp build/ProfilePublisher/ProfilePublisher /opt/crawler/bin/

# Runtime stage (Debian slim)
FROM --platform=${TARGETPLATFORM} debian:bookworm-slim AS runner

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       ca-certificates \
       libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /opt/crawler/bin/ /usr/local/bin/

ENTRYPOINT ["/usr/local/bin/HTMLCrawler"]
