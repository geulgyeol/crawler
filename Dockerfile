ARG BUILDPLATFORM
ARG TARGETPLATFORM
ARG TARGETARCH
ARG TARGETOS

# Builder stage
FROM --platform=${BUILDPLATFORM} alpine:3.20 AS builder

# Install build dependencies
RUN apk add --no-cache \
    bash \
    build-base \
    cmake \
    ninja \
    pkgconfig \
    git \
    curl \
    ca-certificates \
    openssl-dev \
    tar \
    zip \
    unzip

# Install vcpkg for dependencies (musl triplet for Alpine)
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_DEFAULT_TRIPLET=x64-linux-musl \
    VCPKG_BINARY_SOURCES="clear;default" \
    VCPKG_FEATURE_FLAGS=manifests

RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# Install required libraries
RUN ${VCPKG_ROOT}/vcpkg install google-cloud-cpp[pubsub]:x64-linux-musl curl:x64-linux-musl --clean-after-build

WORKDIR /src

COPY . .

# Configure and build with vcpkg toolchain
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=${VCPKG_DEFAULT_TRIPLET} \
    -DVCPKG_LIBRARY_LINKAGE=static \
    && cmake --build build --parallel

# Collect binaries
RUN mkdir -p /opt/crawler/bin \
    && cp build/HTMLCrawler/HTMLCrawler /opt/crawler/bin/ \
    && cp build/LinkFinder/LinkFinder /opt/crawler/bin/ \
    && cp build/ProfileFinder/ProfileFinder /opt/crawler/bin/

# Runtime stage (Alpine)
FROM --platform=${TARGETPLATFORM} alpine:3.20 AS runner

RUN apk add --no-cache \
    ca-certificates \
    libstdc++

WORKDIR /app

COPY --from=builder /opt/crawler/bin/ /usr/local/bin/

ENTRYPOINT ["/usr/local/bin/HTMLCrawler"]
