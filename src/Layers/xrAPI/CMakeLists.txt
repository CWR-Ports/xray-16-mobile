add_library(xrAPI)

target_sources(xrAPI PRIVATE
    stdafx.h
    stdafx.cpp
    xrAPI.cpp
)

target_include_directories(xrAPI
    PRIVATE
    "${XRAY_SOURCE_DIR}/src"
)

target_compile_definitions(xrAPI
    PRIVATE
    XRAPI_EXPORTS
)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    set_target_properties(xrAPI PROPERTIES
        PREFIX ""
    )
endif()

target_precompile_headers(xrAPI
    PRIVATE
    stdafx.h
)

if (BUILD_SHARED_LIBS)
    install(TARGETS xrAPI LIBRARY
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()
