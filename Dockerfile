FROM debian:13-slim

ARG SDL2_VERSION=2.30.10
ENV SDL2_DIR=/opt/SDL2-${SDL2_VERSION}/i686-w64-mingw32

RUN apt-get update && apt-get install -y --no-install-recommends \
  curl \
  ca-certificates \
  gcc-mingw-w64-i686 g++-mingw-w64-i686 cmake make \
  && rm -rf /var/lib/apt/lists/*

# Very confusing SDL2 download and build. Build? So we can use mingw and whatever...
RUN curl -fsSL \
  https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-mingw.tar.gz \
      | tar xz -C /tmp \
  && mkdir -p ${SDL2_DIR}/bin ${SDL2_DIR}/lib/cmake/SDL2 ${SDL2_DIR}/lib/pkgconfig \
  && make -C /tmp/SDL2-${SDL2_VERSION} install-package \
       arch=i686-w64-mingw32 prefix=${SDL2_DIR} \
  && rm -rf /tmp/SDL2-${SDL2_VERSION}

WORKDIR /build
CMD ["./build_mingw.sh"]

