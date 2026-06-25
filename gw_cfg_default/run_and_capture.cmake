# Helper script: runs the gw_cfg_default executable and writes its stdout
# to GW_CFG_DEFAULT_OUT. Fails the build if the executable returns non-zero.
#
# Required variables (passed via -D on the command line):
#   GW_CFG_DEFAULT_EXE - absolute path to the built gw_cfg_default executable
#   GW_CFG_DEFAULT_OUT - absolute path to the output JSON file

if(NOT GW_CFG_DEFAULT_EXE)
    message(FATAL_ERROR "GW_CFG_DEFAULT_EXE is not set")
endif()
if(NOT GW_CFG_DEFAULT_OUT)
    message(FATAL_ERROR "GW_CFG_DEFAULT_OUT is not set")
endif()
if(NOT EXISTS "${GW_CFG_DEFAULT_EXE}")
    message(FATAL_ERROR "gw_cfg_default executable not found: ${GW_CFG_DEFAULT_EXE}")
endif()

find_program(JQ_EXECUTABLE jq)
if(NOT JQ_EXECUTABLE)
    message(FATAL_ERROR "jq is required to generate ${GW_CFG_DEFAULT_OUT} but was not found in PATH")
endif()

message(STATUS "Running ${GW_CFG_DEFAULT_EXE} | jq -> ${GW_CFG_DEFAULT_OUT}")

# jq filter: pretty-print and blank out mqtt_prefix / mqtt_client_id.
set(_jq_filter ".mqtt_prefix = \"\" | .mqtt_client_id = \"\"")

execute_process(
        COMMAND "${GW_CFG_DEFAULT_EXE}"
        COMMAND "${JQ_EXECUTABLE}" "${_jq_filter}"
        OUTPUT_FILE "${GW_CFG_DEFAULT_OUT}"
        RESULTS_VARIABLE _gw_cfg_default_results
        ERROR_VARIABLE   _gw_cfg_default_stderr
)

foreach(_res IN LISTS _gw_cfg_default_results)
    if(NOT "${_res}" STREQUAL "0")
        message(FATAL_ERROR
                "gw_cfg_default pipeline failed (results: ${_gw_cfg_default_results}):\n"
                "${_gw_cfg_default_stderr}")
    endif()
endforeach()

