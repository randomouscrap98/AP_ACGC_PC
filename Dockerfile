FROM debian:13-slim

ARG SDL2_VERSION=2.30.10
ENV SDL2_DIR=/opt/SDL2-${SDL2_VERSION}/i686-w64-mingw32

RUN apt-get update && apt-get install -y --no-install-recommends \
  curl \
  ca-certificates \
  gcc-mingw-w64-i686 g++-mingw-w64-i686 cmake make \
  && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL \
  https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-mingw.tar.gz \
      | tar xz -C /opt

WORKDIR /build
CMD ["./build_mingw.sh"]

