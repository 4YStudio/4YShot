# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "4yshot_autogen"
  "CMakeFiles/4yshot_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/4yshot_autogen.dir/ParseCache.txt"
  )
endif()
