# =============================================================================
# Archipelago client DLL (apclientpp + websocketpp/asio + OpenSSL + zlib)
# =============================================================================
# Built as its own shared library so its C++ runtime and operator new are
# separate from the game's (the game replaces global operator new with JKRHeap,
# which does not exist until after startup). The game only sees the C API in
# include/ap_archipelago.h.
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
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    find_package(OpenSSL REQUIRED)
    set(AP_OPENSSL_INCLUDE "")
    set(AP_OPENSSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

# -----------------------------------------------------------------------------
# zlib (websocket permessage-deflate; Archipelago is moving to require it)
# -----------------------------------------------------------------------------
if(WIN32)
    # libz-mingw-w64-dev (see Dockerfile) installs into the toolchain sysroot
    find_library(ZLIB_STATIC_LIB libz.a REQUIRED)
    set(AP_ZLIB_LIBS ${ZLIB_STATIC_LIB})
else()
    set(ZLIB_USE_STATIC_LIBS ON)
    find_package(ZLIB REQUIRED)
    set(AP_ZLIB_LIBS ZLIB::ZLIB)
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
# slot_data upgrade steps: the apworld's json, embedded (embed_upgrades.cmake)
set(AP_UPGRADES_JSON "${CMAKE_CURRENT_SOURCE_DIR}/../apworld/animal_crossing/slot_data_upgrades.json")
set(AP_UPGRADES_H "${CMAKE_CURRENT_BINARY_DIR}/generated/ap_slotdata_upgrades.h")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${AP_UPGRADES_H}"
    COMMAND ${CMAKE_COMMAND} -DIN=${AP_UPGRADES_JSON} -DOUT=${AP_UPGRADES_H}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_upgrades.cmake"
    DEPENDS "${AP_UPGRADES_JSON}" "${CMAKE_CURRENT_SOURCE_DIR}/embed_upgrades.cmake"
)

# Text client fonts (lib/fonts/README.txt), one embedded array each (embed_font.cmake): file|array|header
set(AP_UI_FONTS
    "NotoSans-Regular.ttf|AP_UI_FONT|ap_ui_font.h"
    "NotoSansSymbols2-subset.ttf|AP_UI_FONT_SYMBOLS2|ap_ui_font_symbols2.h"
    "NotoSansSymbols-subset.ttf|AP_UI_FONT_SYMBOLS|ap_ui_font_symbols.h"
    "NotoSansMath-subset.ttf|AP_UI_FONT_MATH|ap_ui_font_math.h"
)
set(AP_UI_FONT_HEADERS "")
foreach(entry IN LISTS AP_UI_FONTS)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 font_file)
    list(GET parts 1 font_name)
    list(GET parts 2 font_header)
    set(font_in "${CMAKE_CURRENT_SOURCE_DIR}/lib/fonts/${font_file}")
    set(font_out "${CMAKE_CURRENT_BINARY_DIR}/generated/${font_header}")
    add_custom_command(OUTPUT "${font_out}"
        COMMAND ${CMAKE_COMMAND} -DIN=${font_in} -DOUT=${font_out} -DNAME=${font_name}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_font.cmake"
        DEPENDS "${font_in}" "${CMAKE_CURRENT_SOURCE_DIR}/embed_font.cmake"
    )
    list(APPEND AP_UI_FONT_HEADERS "${font_out}")
endforeach()

# Dear ImGui (lib/imgui/VERSION.txt) for the text client (src/ap_ui.cpp): core + the OpenGL 3 backend
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(ap_client SHARED src/ap_archipelago.cpp src/ap_log.cpp src/ap_slotdata.cpp src/ap_messages.cpp
    src/ap_ui.cpp src/ap_ui_log.cpp src/ap_ui_cmdline.cpp src/ap_ui_lists.cpp ${IMGUI_SOURCES} "${AP_UPGRADES_H}" ${AP_UI_FONT_HEADERS})

target_include_directories(ap_client
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include   # C API header, used by the game
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated          # ap_slotdata_upgrades.h, ap_ui_font*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src                # ap_imconfig.h
        ${IMGUI_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/apclientpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/wswrap/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/json/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/ini
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/valijson/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocketpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/asio/include
        ${AP_OPENSSL_INCLUDE}
)

target_compile_definitions(ap_client PRIVATE
    AP_CLIENT_EXPORTS
    "IMGUI_USER_CONFIG=\"ap_imconfig.h\""
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_THREAD_
    $<$<BOOL:${WIN32}>:_WIN32_WINNT=0x0601>
)
target_compile_options(ap_client PRIVATE -O2 -Wno-deprecated-declarations)
if(MINGW)
    # The template-heavy headers exceed the PE object 32k section limit
    target_compile_options(ap_client PRIVATE -Wa,-mbig-obj)
endif()
target_link_libraries(ap_client PRIVATE ${AP_OPENSSL_LIBS} ${AP_ZLIB_LIBS})

if(WIN32)
    # ap_client.dll, not libap_client.dll; static runtime so no extra DLLs
    set_target_properties(ap_client PROPERTIES PREFIX "")
    target_link_options(ap_client PRIVATE -static)
else()
    set_target_properties(ap_client PROPERTIES BUILD_RPATH "$ORIGIN")
    # Own static C++ runtime with local binding, like the DLL: otherwise ELF binds the .so's
    # operator new to the game's (JKRHeap), which is unusable before main()
    target_link_options(ap_client PRIVATE -static-libgcc -static-libstdc++
        -Wl,--exclude-libs,ALL -Wl,-Bsymbolic)
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
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/ini/LICENSE|ini-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/valijson/LICENSE|valijson-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/websocketpp/COPYING|websocketpp-COPYING.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/asio/LICENSE_1_0.txt|asio-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/zlib/LICENSE|zlib-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/cacert/LICENSE-MPL-2.0.txt|cacert-MPL-2.0.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui/LICENSE.txt|imgui-LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/fonts/NotoSans-OFL.txt|NotoSans-OFL.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/fonts/NotoSansSymbols2-OFL.txt|NotoSansSymbols2-OFL.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/fonts/NotoSansSymbols-OFL.txt|NotoSansSymbols-OFL.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/fonts/NotoSansMath-OFL.txt|NotoSansMath-OFL.txt"
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

# Offline mode template (see the file's _readme), next to the executable
add_custom_command(TARGET ap_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/offline_example.json" "${CMAKE_BINARY_DIR}/bin/offline_example.json"
)

install(TARGETS ap_client RUNTIME DESTINATION bin LIBRARY DESTINATION bin)
