# =============================================================================
# Archipelago client DLL (apclientpp + websocketpp/asio + OpenSSL, no zlib)
# =============================================================================
# Built as its own shared library so its C++ runtime and operator new are
# separate from the game's (the game replaces global operator new with JKRHeap,
# which does not exist until after startup). The game only sees the C API in
# include/pc_archipelago.h.
#
# NOTE: this directory is added before the game's include/compile settings so
# none of them (decomp headers, TARGET_PC, etc.) leak into this target.

# -----------------------------------------------------------------------------
# OpenSSL (wss:// support)
# -----------------------------------------------------------------------------
if(WIN32)
    # Static OpenSSL built for MinGW (see Dockerfile). Bypass find_package: the
    # cross toolchain restricts find paths to /usr/i686-w64-mingw32.
    set(OPENSSL_ROOT_DIR "" CACHE PATH "MinGW OpenSSL install prefix")
    find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h HINTS "${OPENSSL_ROOT_DIR}/include"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_library(OPENSSL_SSL_LIB libssl.a HINTS "${OPENSSL_ROOT_DIR}/lib"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_library(OPENSSL_CRYPTO_LIB libcrypto.a HINTS "${OPENSSL_ROOT_DIR}/lib"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    if(NOT OPENSSL_INCLUDE_DIR OR NOT OPENSSL_SSL_LIB OR NOT OPENSSL_CRYPTO_LIB)
        message(FATAL_ERROR
            "MinGW OpenSSL not found! Set -DOPENSSL_ROOT_DIR=<prefix> "
            "(build via build_podman.sh, or see the Dockerfile).")
    endif()
    set(AP_OPENSSL_INCLUDE ${OPENSSL_INCLUDE_DIR})
    set(AP_OPENSSL_LIBS ${OPENSSL_SSL_LIB} ${OPENSSL_CRYPTO_LIB}
        ws2_32 mswsock crypt32 gdi32 advapi32 user32)
else()
    find_package(OpenSSL REQUIRED)
    set(AP_OPENSSL_INCLUDE "")
    set(AP_OPENSSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

# -----------------------------------------------------------------------------
# CA bundle (cacert.pem) for wss:// certificate validation
# -----------------------------------------------------------------------------
# Downloaded from curl.se at configure time (SHA256-verified) and placed next
# to the executable. The client passes "cacert.pem" as the cert store path.
set(CACERT_PATH "${CMAKE_BINARY_DIR}/bin/cacert.pem")
if(NOT EXISTS "${CACERT_PATH}")
    file(DOWNLOAD "https://curl.se/ca/cacert.pem.sha256" "${CMAKE_BINARY_DIR}/cacert.pem.sha256"
        STATUS CACERT_HASH_STATUS)
    list(GET CACERT_HASH_STATUS 0 CACERT_HASH_CODE)
    if(CACERT_HASH_CODE EQUAL 0)
        file(READ "${CMAKE_BINARY_DIR}/cacert.pem.sha256" CACERT_HASH_LINE)
        string(SUBSTRING "${CACERT_HASH_LINE}" 0 64 CACERT_SHA256)
        file(DOWNLOAD "https://curl.se/ca/cacert.pem" "${CACERT_PATH}"
            EXPECTED_HASH SHA256=${CACERT_SHA256} STATUS CACERT_STATUS)
        list(GET CACERT_STATUS 0 CACERT_CODE)
        if(NOT CACERT_CODE EQUAL 0)
            file(REMOVE "${CACERT_PATH}")
            message(WARNING "Could not download cacert.pem (${CACERT_STATUS}); wss:// will fall back to system certs")
        endif()
    else()
        message(WARNING "Could not download cacert.pem.sha256 (${CACERT_HASH_STATUS}); wss:// will fall back to system certs")
    endif()
endif()

# -----------------------------------------------------------------------------
# ap_client shared library
# -----------------------------------------------------------------------------
add_library(ap_client SHARED src/pc_archipelago.cpp)

target_include_directories(ap_client
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include   # C API header, used by the game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/apclientpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/wswrap/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/json/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/valijson/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocketpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/asio/include
        ${AP_OPENSSL_INCLUDE}
)

target_compile_definitions(ap_client PRIVATE
    AP_CLIENT_EXPORTS
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_THREAD_
    WSWRAP_NO_COMPRESSION
    $<$<BOOL:${WIN32}>:_WIN32_WINNT=0x0601>
)
target_compile_options(ap_client PRIVATE -O2 -Wno-deprecated-declarations)
if(MINGW)
    # The template-heavy headers exceed the PE object 32k section limit
    target_compile_options(ap_client PRIVATE -Wa,-mbig-obj)
endif()
target_link_libraries(ap_client PRIVATE ${AP_OPENSSL_LIBS})

if(WIN32)
    # ap_client.dll, not libap_client.dll; static runtime so no extra DLLs
    set_target_properties(ap_client PROPERTIES PREFIX "")
    target_link_options(ap_client PRIVATE -static)
else()
    set_target_properties(ap_client PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

set_target_properties(ap_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# -----------------------------------------------------------------------------
# Third-party license texts, shipped next to the executable
# -----------------------------------------------------------------------------
set(THIRD_PARTY_LICENSES
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/apclientpp/LICENSE|apclientpp-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/wswrap/LICENSE|wswrap-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/json/LICENSE.MIT|nlohmann-json-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/valijson/LICENSE|valijson-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/websocketpp/COPYING|websocketpp-COPYING.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/asio/LICENSE_1_0.txt|asio-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/cacert/LICENSE-MPL-2.0.txt|cacert-MPL-2.0.txt"
)
foreach(entry IN LISTS THIRD_PARTY_LICENSES)
    string(REPLACE "|" ";" pair "${entry}")
    list(GET pair 0 lic_src)
    list(GET pair 1 lic_dst)
    add_custom_command(TARGET ap_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${lic_src}" "${CMAKE_BINARY_DIR}/bin/licenses/${lic_dst}"
    )
endforeach()

install(TARGETS ap_client RUNTIME DESTINATION bin LIBRARY DESTINATION bin)
