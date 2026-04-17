cmake_minimum_required(VERSION 3.5)
project(jabcode C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)

if(NOT WIN32)
    find_library(M_LIB m)
endif()

find_package(TIFF)
find_package(PNG)

if(NOT TIFF_FOUND)
    message(WARNING "libtiff not found, will probably end in an error\n"
                    "Try installing libtiff-dev or similar from your package manager")
endif()
if(NOT PNG_FOUND)
    message(WARNING "libpng not found, will probably end in an error\n"
                    "Try installing libpng-dev or similar from your package manager")
endif()

add_library(jabcode
    src/jabcode/binarizer.c
    src/jabcode/decoder.c
    src/jabcode/decoder.h
    src/jabcode/detector.c
    src/jabcode/detector.h
    src/jabcode/encoder.c
    src/jabcode/encoder.h
    src/jabcode/image.c
    src/jabcode/include/jabcode.h
    src/jabcode/interleave.c
    src/jabcode/ldpc.c
    src/jabcode/ldpc.h
    src/jabcode/mask.c
    src/jabcode/pseudo_random.c
    src/jabcode/pseudo_random.h
    src/jabcode/sample.c
    src/jabcode/transform.c)

target_link_libraries(jabcode PUBLIC
    PNG::PNG
    TIFF::TIFF
    ${M_LIB})
target_include_directories(jabcode PUBLIC
    src/jabcode/include
    ${PNG_INCLUDE_DIRS}
    ${TIFF_INCLUDE_DIR})

add_executable(jabcodeReader
    src/jabcodeReader/jabreader.c)
target_link_libraries(jabcodeReader PRIVATE
    jabcode)

add_executable(jabcodeWriter
    src/jabcodeWriter/jabwriter.c
    src/jabcodeWriter/jabwriter.h)
target_link_libraries(jabcodeWriter PRIVATE
    jabcode)

install(TARGETS jabcode jabcodeReader jabcodeWriter
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES src/jabcode/include/jabcode.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
