project(foo_dsp_silence)
cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 20)

# foobar2000-sdk provides CMake targets:
# 
#     # this is heuristically generated, and may not be correct
#     find_package(unofficial-foobar2000-sdk CONFIG REQUIRED)
#     # note: 2 additional targets are not displayed.
#     target_link_libraries(main PRIVATE unofficial::foobar2000-sdk::SDK unofficial::foobar2000-sdk::pfc unofficial::foobar2000-sdk::shared unofficial::foobar2000-sdk::helpers)

# foobar2000
find_package(unofficial-foobar2000-sdk CONFIG REQUIRED)

add_compile_options(-MP)

add_compile_definitions(UNICODE _UNICODE)

set(DSP_SOURCES
"foo_dsp_silence.cpp"
"foo_dsp_silence.rc"
"resource.h"
)

add_library(foo_dsp_silence SHARED
	${DSP_SOURCES}
)

target_link_libraries(foo_dsp_silence
  "shlwapi.lib"
  unofficial::foobar2000-sdk::foobar2000_component_client
  unofficial::foobar2000-sdk::helpers
  unofficial::foobar2000-sdk::libPPUI
  unofficial::foobar2000-sdk::pfc
  unofficial::foobar2000-sdk::SDK
  unofficial::foobar2000-sdk::shared
)
