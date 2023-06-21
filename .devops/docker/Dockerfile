# syntax=docker/dockerfile:experimental

FROM gcc:11.2.0 as setup-stage

ARG LOGPASS_ENV
ARG USER_ID=1000
# Possible values are empty, `Debug`, `Release`, `RelWithDebInfo`
# and `MinSizeRel`
ARG BUILD_TYPE=Debug

ENV LOGPASS_ENV=${LOGPASS_ENV} \
  # build settings
  BUILD_TYPE=${BUILD_TYPE} \
  # user
  USER_ID=${USER_ID} \
  USER_HOME=/home/logpass

# install essential dependencies
RUN apt-get --quiet=2 update \
  && apt-get --quiet=2 --yes --no-install-recommends install gdb tini \
  && apt-get --quiet=2 --yes autoremove \
  && apt-get --quiet=2 --yes clean \
  && rm -rf /var/lib/apt/lists/*

# create the user and group to run the application as
RUN addgroup --system --gid "${USER_ID}" logpass \
  && adduser \
    --system \
    --home "${USER_HOME}" \
    --uid "${USER_ID}" \
    --ingroup logpass \
  logpass \
  && touch "${USER_HOME}/.bashrc"


FROM setup-stage as development-stage

ENV CMAKE_VERSION=3.22.3 \
  CMAKE_CHECKSUM=96ace92dfe42b1c6c9c116fdb1c7adc42cb90cbaa87adcebc1a8de7ea129865c \
  VCPKG_VERSION=2023.06.20 \
  VCPKG_CHECKSUM=d94456e6d17e31051663a4e909ca5627882d9589649084ef2db6492a6405d1cc \
  VCPKG_FEATURE_FLAGS=binarycaching \
  VCPKG_BINARY_SOURCES="clear;files,/opt/vcpkg/archives,readwrite" \
  VCPKG_DISABLE_METRICS=ON \
  VCPKG_CMAKE_PATH="/opt/vcpkg/scripts/buildsystems/vcpkg.cmake"

# TODO(skarzi): consider add `--no-install-recommends` to `apt-get install`
RUN apt-get --quiet=2 update \
  && apt-get --quiet=2 --yes install \
    bash \
    build-essential \
    curl \
    git \
    make \
    ninja-build \
    pkg-config \
    tar \
    unzip \
    zip \
  # cleaning apt cache
  && apt-get --quiet=2 --yes autoremove \
  && apt-get --quiet=2 --yes clean \
  && rm -rf /var/lib/apt/lists/* \
  # install vcpkg
  && curl \
    --location \
      "https://github.com/microsoft/vcpkg/archive/${VCPKG_VERSION}.tar.gz" \
    --retry 3 \
    --output "/tmp/vcpkg-${VCPKG_VERSION}.tar.gz" \
  && echo "${VCPKG_CHECKSUM} /tmp/vcpkg-${VCPKG_VERSION}.tar.gz" \
    | sha256sum --check \
  && tar --directory /tmp/ -xzvf "/tmp/vcpkg-${VCPKG_VERSION}.tar.gz" \
  && mv "/tmp/vcpkg-${VCPKG_VERSION}" /opt/vcpkg \
  && bash /opt/vcpkg/bootstrap-vcpkg.sh \
  && ln --symbolic /opt/vcpkg/vcpkg /usr/bin/vcpkg \
  && rm -rf "/tmp/vcpkg-${VCPKG_VERSION}.tar.gz" \
  # install CMake
  && curl \
    --location \
      "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz" \
    --retry 3 \
    --output "/tmp/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
  && echo "${CMAKE_CHECKSUM} /tmp/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
    | sha256sum --check \
  && tar \
    --directory /tmp/ \
    -xzf "/tmp/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
  && mv "/tmp/cmake-${CMAKE_VERSION}-linux-x86_64" /opt/cmake \
  && chmod +x /opt/cmake/bin/cmake \
  && ln --symbolic /opt/cmake/bin/cmake /usr/bin/cmake \
  && rm -rf "/tmp/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz"

# add logpass fork of rocksdb and iroha-ed25519 to vcpkg
RUN rm -rf /opt/vcpkg/ports/rocksdb
COPY ./vcpkg/rocksdb /opt/vcpkg/ports/rocksdb
COPY ./vcpkg/iroha-ed25519 /opt/vcpkg/ports/iroha-ed25519

# install logpass node dependencies
# TODO(skarzi): benefit from `--mount=type=cache` not only when building images
# locally when following issue will be resolved:
# https://github.com/moby/buildkit/issues/1512
RUN mkdir -p /opt/vcpkg/archives
RUN --mount=type=cache,target=/opt/vcpkg/archives,id=logpass-vcpkg-binarycache \
  # TODO(skarzi): create manifest file with all dependencies moved there
  # https://github.com/microsoft/vcpkg/blob/master/docs/specifications/manifests.md
  vcpkg install \
    boost-algorithm \
    boost-asio \
    boost-bimap \
    boost-endian \
    boost-exception \
    boost-filesystem \
    boost-log \
    boost-program-options \
    boost-stacktrace \
    boost-system \
    boost-test \
    cppcodec \
    iroha-ed25519 \
    nlohmann-json \
    openssl \
    rocksdb \
    usockets \
    uwebsockets \
  && rm -rf \
    /opt/vcpkg/buildtrees \
    /opt/vcpkg/downloads \
    /opt/vcpkg/packages

WORKDIR /app
RUN mkdir -p /app/build \
  && chown -R logpass:logpass /app /opt/vcpkg "${USER_HOME}"

COPY ./.devops/docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

USER logpass
ENTRYPOINT ["tini", "-p",  "SIGTERM", "--", "/entrypoint.sh"]


FROM development-stage as build-stage

COPY . /app/
RUN cmake \
        -B build \
        -D CMAKE_TOOLCHAIN_FILE="${VCPKG_CMAKE_PATH}" \
        -G "Ninja Multi-Config" \
    && cmake \
      --build build \
      --config "${BUILD_TYPE}" \
      --parallel \
      --target node


FROM setup-stage as production-stage

USER logpass
WORKDIR /app
COPY --from=build-stage "/app/build/${BUILD_TYPE}/node" /usr/local/bin/

ENTRYPOINT ["tini", "-p",  "SIGTERM", "--"]
# full path is required for correct debugging 
CMD ["/usr/local/bin/node"]
