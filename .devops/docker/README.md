# Logpass node

Image with logpass blockchain node.

## Tags naming convention

Following table present tags naming convention with short description
and examples:

| Pattern                              | Context                                 | Example                                  |
|--------------------------------------|-----------------------------------------|------------------------------------------|
| latest{optional-suffix}              | Latest image, built from `main` branch  | latest                                   |
| {git-commit-sha}{optional-suffix}    | Image built for given git commit hash   | 1016c7abca369216770044f620f6ef2efe300a26 |
| v{semantic-version}{optional-suffix} | Image build for given git tag (release) | v1.0.24                                  |
| ci                                   | Image used to test node in CI pipeline  | ci                                       |
| cache{optional-suffix}               | Image with inline cache                 | cache-debug                              |

Images built without suffixes are considered as production ones
(built with `RelWithDebInfo` config). All currently supported suffixes:

- `-debug` - image built with `Debug` config

## How to use this image?

This image can be configured using one of following options:

+ CLI arguments, e.g. `--api-host=0.0.0.0`
+ environment variables e.g. `LOGPASS__API_HOST=0.0.0.0`. Note that all 
  `logpass` related env variables are prefixed with `LOGPASS__`
+ `config.ini` file (see
  [`config.ini.example`](https://github.com/LogPass/node/blob/main/config.ini.example)
  ). This file should be mounted to `/app/config.ini` target


For example to run logpass `node` `latest` image using host's `config.ini` use 
following command:

```bash
docker run \
  -it \
  --rm \
  --name my-node-run \
  --mount "type=bind,source=$(pwd)/config.ini,target=/app/conig.ini" \
logpass/node:latest
```
