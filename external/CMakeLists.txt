# set up the external libraries that aren't popular enough to be found on most
# package managers are included with the project (in the 'external' folder)

# Keep NanoSVG available while svg-modern is being validated. TextureData now
# uses PlutoSVG, but retaining this target makes rollback/A-B testing trivial.
add_subdirectory("nanosvg")

# PlutoVG / PlutoSVG are pinned as submodules. Build them with small local CMake
# definitions instead of their upstream CMakeLists so ES-X can keep its older
# CMake baseline (upstream currently requires CMake 3.15).
set(PLUTOVG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-blend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-canvas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-font.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-matrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-paint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-path.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-rasterize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-surface.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-ft-math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-ft-raster.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/plutovg-ft-stroker.c
)

add_library(plutovg STATIC ${PLUTOVG_SOURCES})
add_library(plutovg::plutovg ALIAS plutovg)
set_target_properties(plutovg PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
target_include_directories(plutovg
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/include
)
target_compile_definitions(plutovg PRIVATE PLUTOVG_BUILD PUBLIC PLUTOVG_BUILD_STATIC)

include(CheckIncludeFile)
check_include_file("threads.h" PLUTOVG_HAVE_THREADS_H)
if(PLUTOVG_HAVE_THREADS_H)
    target_compile_definitions(plutovg PRIVATE HAVE_THREADS_H)
endif()

find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(plutovg PRIVATE Threads::Threads)
endif()

find_library(PLUTOVG_MATH_LIBRARY m)
if(PLUTOVG_MATH_LIBRARY)
    target_link_libraries(plutovg PRIVATE ${PLUTOVG_MATH_LIBRARY})
endif()

add_library(plutosvg STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/plutosvg/source/plutosvg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/plutosvg/source/plutosvg.h
)
add_library(plutosvg::plutosvg ALIAS plutosvg)
set_target_properties(plutosvg PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
target_include_directories(plutosvg
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plutosvg/source
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/plutosvg/source
)
target_compile_definitions(plutosvg PRIVATE PLUTOSVG_BUILD PUBLIC PLUTOSVG_BUILD_STATIC)
target_link_libraries(plutosvg PUBLIC plutovg)

find_library(PLUTOSVG_MATH_LIBRARY m)
if(PLUTOSVG_MATH_LIBRARY)
    target_link_libraries(plutosvg PRIVATE ${PLUTOSVG_MATH_LIBRARY})
endif()

find_package(pugixml)

if(NOT pugixml_FOUND)
  add_subdirectory("pugixml")
endif()
