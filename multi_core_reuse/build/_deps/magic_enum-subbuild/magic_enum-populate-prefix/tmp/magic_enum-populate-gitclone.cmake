
if(NOT "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/magic_enum-populate-gitinfo.txt" IS_NEWER_THAN "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/magic_enum-populate-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: '/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/magic_enum-populate-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/magic_enum/4d76fe0a5b27a0e62d6c15976d02b33c54207096"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/magic_enum/4d76fe0a5b27a0e62d6c15976d02b33c54207096'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"  clone --no-checkout --depth 1 --no-single-branch --config "advice.detachedHead=false" "https://github.com/Neargye/magic_enum.git" "4d76fe0a5b27a0e62d6c15976d02b33c54207096"
    WORKING_DIRECTORY "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/magic_enum"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/Neargye/magic_enum.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git"  checkout v0.9.7 --
  WORKING_DIRECTORY "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/magic_enum/4d76fe0a5b27a0e62d6c15976d02b33c54207096"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'v0.9.7'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git"  submodule update --recursive --init 
    WORKING_DIRECTORY "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/magic_enum/4d76fe0a5b27a0e62d6c15976d02b33c54207096"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/magic_enum/4d76fe0a5b27a0e62d6c15976d02b33c54207096'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/magic_enum-populate-gitinfo.txt"
    "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/magic_enum-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/magic_enum-populate-gitclone-lastrun.txt'")
endif()

