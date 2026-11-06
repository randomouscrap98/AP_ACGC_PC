FROM debian:13-slim

ARG SDL3_VERSION=3.4.20
ENV SDL3_DIR=/opt/SDL3-${SDL3_VERSION}/i686-w64-mingw32
# Latest 3.5.x LTS: websocketpp/asio are not verified against OpenSSL 4.x
ARG OPENSSL_VERSION=3.5.8
ENV OPENSSL_DIR=/opt/openssl-${OPENSSL_VERSION}/i686-w64-mingw32

RUN apt-get update && apt-get install -y --no-install-recommends \
  curl \
  ca-certificates \
  gcc-mingw-w64-i686 g++-mingw-w64-i686 libz-mingw-w64-dev cmake make perl \
  glslang-tools \
  && update-alternatives --set i686-w64-mingw32-gcc /usr/bin/i686-w64-mingw32-gcc-posix \
  && update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix \
  && rm -rf /var/lib/apt/lists/*

# SDL3 MinGW development package: prebuilt SDL3.dll, headers and a relocatable CMake config
RUN curl -fsSL --retry 5 --retry-all-errors -o /tmp/sdl3.tar.gz \
  https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-devel-${SDL3_VERSION}-mingw.tar.gz \
  && tar xzf /tmp/sdl3.tar.gz -C /tmp \
  && rm /tmp/sdl3.tar.gz \
  && mkdir -p /opt/SDL3-${SDL3_VERSION} \
  && cp -r /tmp/SDL3-${SDL3_VERSION}/i686-w64-mingw32 ${SDL3_DIR} \
  && cp /tmp/SDL3-${SDL3_VERSION}/LICENSE.txt ${SDL3_DIR}/LICENSE.txt \
  && rm -rf /tmp/SDL3-${SDL3_VERSION}

# Static OpenSSL for wss:// support in apclientpp (no DLLs to ship)
RUN cd /tmp \
  && curl -fsSLO --retry 5 --retry-all-errors https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz \
  && curl -fsSLO --retry 5 --retry-all-errors https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz.sha256 \
  && echo "$(cut -d' ' -f1 openssl-${OPENSSL_VERSION}.tar.gz.sha256)  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c - \
  && tar xzf openssl-${OPENSSL_VERSION}.tar.gz \
  && cd openssl-${OPENSSL_VERSION} \
  && ./Configure mingw --cross-compile-prefix=i686-w64-mingw32- \
       no-shared no-tests no-docs no-asm --libdir=lib --prefix=${OPENSSL_DIR} \
  && make -j"$(nproc)" build_sw \
  && make install_sw \
  && cp LICENSE.txt ${OPENSSL_DIR}/LICENSE.txt \
  && cd / && rm -rf /tmp/openssl-${OPENSSL_VERSION}*

WORKDIR /build
CMD ["./build_mingw.sh"]

