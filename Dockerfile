# syntax=docker/dockerfile:1
FROM debian:12-slim AS app-build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build zlib1g-dev libmariadb-dev \
    libboost-system-dev libtinyxml-dev libutfcpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt config.h.in ./
COPY cmake/ cmake/
COPY dep/ dep/
COPY src/ src/
COPY utils/ utils/

ARG BUILD_JOBS=2
ARG EVEMU_BUILD_TESTS=OFF
RUN cmake -S . -B build -G Ninja -DEVEMU_BUILD_TESTS="$EVEMU_BUILD_TESTS" \
    -DCMAKE_INSTALL_PREFIX=/app -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel "${BUILD_JOBS}" \
    && if [ "$EVEMU_BUILD_TESTS" = "ON" ]; then ctest --test-dir build -R "^(MarketOrderReaderTest|RefPtrLifetimeTest)$" --no-tests=error --output-on-failure; fi \
    && cmake --install build

FROM --platform=$BUILDPLATFORM golang:1.23-bookworm AS evedbtool-build
ARG TARGETARCH
WORKDIR /build
RUN git clone --quiet --depth 1 --branch 0.0.6 https://github.com/EvEmu-Project/EVEDBTool.git /build/EVEDBTool
WORKDIR /build/EVEDBTool
COPY tools/evedbtool/command_seed.go command_seed.go
RUN --mount=type=cache,target=/go/pkg/mod --mount=type=cache,target=/root/.cache/go-build \
    CGO_ENABLED=0 GOOS=linux GOARCH=${TARGETARCH:-amd64} \
    go build -trimpath -ldflags "-s -w -X main.version=evemu-crucible" -o /evedbtool .

FROM debian:12-slim AS app
LABEL description="EVEmu Server"
ARG WITH_GDB=0
RUN apt-get update \
    && packages="ca-certificates mariadb-client-core libmariadb3 libstdc++6 zlib1g libboost-system1.74.0 libtinyxml2.6.2v5" \
    && if [ "$WITH_GDB" = "1" ]; then packages="$packages gdb"; fi \
    && apt-get install -y --no-install-recommends $packages \
    && rm -rf /var/lib/apt/lists/* \
    && useradd evemu \
    && command -v setpriv \
    && command -v mariadb
COPY --from=app-build /app/ /app/
COPY utils/container-scripts/ /src/utils/container-scripts/
COPY utils/config/ /src/utils/config/
COPY sql/ /src/sql/
COPY --from=evedbtool-build /evedbtool /src/sql/evedbtool

# Fail the image build if a required shared library was omitted.
RUN ldd /app/bin/eve-server > /tmp/runtime-libs \
    && cat /tmp/runtime-libs \
    && ! grep -q "not found" /tmp/runtime-libs \
    && rm /tmp/runtime-libs

EXPOSE 26000 26001
ENTRYPOINT ["/bin/sh", "/src/utils/container-scripts/entry.sh"]
CMD ["/bin/bash", "/src/utils/container-scripts/start.sh"]
