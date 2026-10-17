# Shared workspace paths must be established before project()/compiler detection.
include_guard(GLOBAL)
get_filename_component(SEIS3D_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
get_filename_component(SEIS3D_WORKSPACE_ROOT "${SEIS3D_REPOSITORY_ROOT}/.." REALPATH)
set(SEIS3D_OUTPUT_ROOT "${SEIS3D_WORKSPACE_ROOT}/out")

# Normalize separators, parent components and Windows drive-letter case.
set(seis3dAllowedBuildRoot "${SEIS3D_OUTPUT_ROOT}")
set(seis3dRequestedBuildRoot "${CMAKE_BINARY_DIR}")
cmake_path(NORMAL_PATH seis3dAllowedBuildRoot)
cmake_path(NORMAL_PATH seis3dRequestedBuildRoot)
if(CMAKE_HOST_WIN32)
    string(TOLOWER "${seis3dAllowedBuildRoot}" seis3dAllowedBuildRoot)
    string(TOLOWER "${seis3dRequestedBuildRoot}" seis3dRequestedBuildRoot)
endif()
cmake_path(IS_PREFIX seis3dAllowedBuildRoot "${seis3dRequestedBuildRoot}" NORMALIZE seis3dBuildInsideOutput)
if(NOT seis3dBuildInsideOutput)
    message(FATAL_ERROR
        "Build files must stay inside ${SEIS3D_OUTPUT_ROOT}.\n"
        "Open the Git repository folder in Visual Studio and select x64-Release or x64-Debug.\n"
        "Command line: cmake -S \"${SEIS3D_REPOSITORY_ROOT}\" -B \"${SEIS3D_OUTPUT_ROOT}/build/x64-Release\"")
endif()

# $<CONFIG> prevents the VS generator from adding a second configuration suffix.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SEIS3D_OUTPUT_ROOT}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SEIS3D_OUTPUT_ROOT}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SEIS3D_OUTPUT_ROOT}/lib/$<CONFIG>")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${SEIS3D_OUTPUT_ROOT}/bin/$<CONFIG>")
set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb/$<CONFIG>")

# Keep compiler debug records in object files instead of shared compile PDBs.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
endif()
