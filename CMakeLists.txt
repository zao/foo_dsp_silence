include_directories(${WTL_INCLUDE_DIRS})

set(DSP_SOURCES
"foo_dsp_silence.cpp"
"foo_dsp_silence.rc"
"resource.h"
)

include_directories(.)
add_definitions(-Zm300 -DNOMINMAX)
add_library(foo_dsp_silence SHARED
	${DSP_SOURCES}
)

target_link_libraries(foo_dsp_silence
	foobar2000_component_client
	helpers
	pfc
	SDK
	shared
	#"comdlg32.lib"
	#"rpcrt4.lib"
	#"winmm.lib"
)
deploy_component(foo_dsp_silence foo_dsp_silence)
