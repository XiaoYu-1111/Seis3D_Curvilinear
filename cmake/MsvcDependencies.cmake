# Keep localized /showIncludes output consistent in VS and command-line Ninja.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set(seis3dMsvcLauncher cmd.exe /d /c call "${CMAKE_CURRENT_LIST_DIR}/MsvcCompile.cmd")
    set(seis3dProbeDir "${CMAKE_BINARY_DIR}/CMakeFiles/Seis3DShowIncludes")
    file(MAKE_DIRECTORY "${seis3dProbeDir}")
    file(WRITE "${seis3dProbeDir}/probe.h" "/* Header dependency probe. */\n")
    file(WRITE "${seis3dProbeDir}/probe.cpp" "#include \"probe.h\"\nint main() { return 0; }\n")
    execute_process(
        COMMAND ${seis3dMsvcLauncher} "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c probe.cpp /Foprobe.obj
        WORKING_DIRECTORY "${seis3dProbeDir}"
        OUTPUT_VARIABLE seis3dIncludeOutput ERROR_VARIABLE seis3dIncludeError
        RESULT_VARIABLE seis3dProbeResult ENCODING UTF-8)
    if(NOT seis3dProbeResult EQUAL 0 OR NOT seis3dIncludeOutput MATCHES "(^|\n)([^\r\n]+: +)[A-Za-z]:[/\\\\][^\r\n]*probe\\.h")
        message(FATAL_ERROR "Cannot detect MSVC header dependencies: ${seis3dIncludeOutput}${seis3dIncludeError}")
    endif()
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
    list(PREPEND CMAKE_CXX_COMPILER_LAUNCHER ${seis3dMsvcLauncher})
endif()
