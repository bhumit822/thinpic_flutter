cmake_minimum_required(VERSION 3.4.1)
project(thinpic_flutter)

set(native_src_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(jni_libs_dir ${native_src_dir}/../jniLibs)
set(include_dir ${native_src_dir}/include)

# Include headers
include_directories(
    ${include_dir}
    ${include_dir}/vips
    ${include_dir}/glib-2.0
    ${include_dir}/gio-unix-2.0
    ${include_dir}/glibconfig
)

# Native source
add_library(thinpic_flutter SHARED
    ${native_src_dir}/image_compressor.c
)

# Link prebuilt dynamic libraries (IMPORTED)
macro(link_prebuilt_so name)
    add_library(${name} SHARED IMPORTED)
    set_target_properties(${name} PROPERTIES
        IMPORTED_LOCATION ${jni_libs_dir}/${ANDROID_ABI}/lib${name}.so
    )
endmacro()

# Core libvips and dependencies
link_prebuilt_so(vips)
link_prebuilt_so(gobject-2.0)
link_prebuilt_so(glib-2.0)
link_prebuilt_so(gmodule-2.0)

# Optional: link more if needed (e.g. fftw3, jpeg, tiff, etc.)
# link_prebuilt_so(fftw3)
# link_prebuilt_so(jpeg)
# ...

# Link to thinpic_flutter
target_link_libraries(thinpic_flutter
    vips
    gobject-2.0
    glib-2.0
    gmodule-2.0
    log
    android
)

# Set compiler flags for better debugging
target_compile_options(thinpic_flutter PRIVATE
    -Wall
    -Wextra
    -O2
    -g
)
