# Logpass node

Implementation of logpass blockchain node

## Local development

Every command listed below should be run from repository root.

Generate private key with `openssl` command:

```bash
openssl genpkey -algorithm ed25519 -outform PEM -out key.pem
```

Prepare custom `config.ini` file for local development:

```bash
cp config.ini.example config.ini
```

Now, adjust all values in `config.ini` (especially `threads`) according to your
preferences.

### Visual Studio

To build logpass `node` with Visual Studio, you need to:

1. Get and install Microsoft Visual Studio 2019 with C++ development option
2. Get, install and integrate [vcpkg](https://github.com/microsoft/vcpkg)
3. Copy directories from project `vcpkg` directory to your `vcpkg/ports`
4. Install vcpkg dependencies:

   On Linux:

   ```bash
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
     uwebsockets
   ```

   On Windows:

   ```bat
   vcpkg install ^
     boost-algorithm ^
     boost-asio ^
     boost-bimap ^
     boost-endian ^
     boost-exception ^
     boost-filesystem ^
     boost-log ^
     boost-program-options ^
     boost-stacktrace ^
     boost-system ^
     boost-test ^
     cppcodec ^
     iroha-ed25519 ^
     nlohmann-json ^
     openssl ^
     rocksdb ^
     usockets ^
     uwebsockets
   ```

5. Open `node.sln`, select configuration and press build

### docker and docker-compose

[`BuildKit`](https://github.com/moby/buildkit) is required to build `node`
image . To enable `BuildKit` builds for `docker` and `docker-compose` let's
export (or even better add it to your `.bashrc`) following environment
variables:

```bash
DOCKER_BUILDKIT=1
COMPOSE_DOCKER_CLI_BUILD=1
```

When `BuildKit` is setup, let's move to create a custom
`docker-compose.override.yml` file:

```bash
cp \
  .devops/docker/docker-compose.override.yml.example \
  docker-compose.override.yml
```

To build `Dockerfile` that won't mess up with your local file permissions you
need to find your current user id - `UID`, by running following command:

```bash
echo $UID
```

Replace all repetitions of `!!UID!!` in `docker-compose.override.yml` with
above found value.

Finally setup
[global docker's network](https://github.com/LogPass/logpass_docker_network):

<!-- markdownlint-disable line-length -->
```bash
curl \
    https://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@raw.githubusercontent.com/LogPass/logpass_docker_network/main/scripts/setup_global_network.sh \
| bash
```
<!-- markdownlint-enable line-length -->

This network is used to connect all LogPass services together when developing
locally.

#### Using `docker-compose`

To build all services simply run:

```bash
docker-compose build
```

To run all services in the background:

```bash
docker-compose up --detach
```

To run `node` tests:

```bash
docker-compose run --rm node ./build/Debug/node_tests
```
