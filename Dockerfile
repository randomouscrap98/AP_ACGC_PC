FROM debian:13-slim

ARG SDL2_VERSION=2.30.10
ENV SDL2_DIR=/opt/SDL2-${SDL2_VERSION}/i686-w64-mingw32
# Latest 3.5.x LTS: websocketpp/asio are not verified against OpenSSL 4.x
ARG OPENSSL_VERSION=3.5.8
ENV OPENSSL_DIR=/opt/openssl-${OPENSSL_VERSION}/i686-w64-mingw32

RUN apt-get update && apt-get install -y --no-install-recommends \
  curl \
  ca-certificates \
  gcc-mingw-w64-i686 g++-mingw-w64-i686 cmake make perl \
  && update-alternatives --set i686-w64-mingw32-gcc /usr/bin/i686-w64-mingw32-gcc-posix \
  && update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix \
  && rm -rf /var/lib/apt/lists/*

# Very confusing SDL2 download and build. Build? So we can use mingw and whatever...
RUN curl -fsSL \
  https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-mingw.tar.gz \
      | tar xz -C /tmp \
  && mkdir -p ${SDL2_DIR}/bin ${SDL2_DIR}/lib/cmake/SDL2 ${SDL2_DIR}/lib/pkgconfig \
  && make -C /tmp/SDL2-${SDL2_VERSION} install-package \
       arch=i686-w64-mingw32 prefix=${SDL2_DIR} \
  && rm -rf /tmp/SDL2-${SDL2_VERSION}

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

