# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\viba_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\viba_autogen.dir\\ParseCache.txt"
  "viba_autogen"
  )
endif()
