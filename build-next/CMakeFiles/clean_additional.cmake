# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  [[CMakeFiles\CppVisualIDE_autogen.dir\AutogenUsed.txt]]
  [[CMakeFiles\CppVisualIDE_autogen.dir\ParseCache.txt]]
  "CppVisualIDE_autogen"
  )
endif()
