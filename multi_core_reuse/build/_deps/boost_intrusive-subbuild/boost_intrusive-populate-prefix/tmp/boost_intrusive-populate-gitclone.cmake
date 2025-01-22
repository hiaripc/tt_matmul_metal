
if(NOT "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/boost_intrusive-subbuild/boost_intrusive-populate-prefix/src/boost_intrusive-populate-stamp/boost_intrusive-populate-gitinfo.txt" IS_NEWER_THAN "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/boost_intrusive-subbuild/boost_intrusive-populate-prefix/src/boost_intrusive-populate-stamp/boost_intrusive-populate-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: '/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/boost_intrusive-subbuild/boost_intrusive-populate-prefix/src/boost_intrusive-populate-stamp/boost_intrusive-populate-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/boost_intrusive/4a7bf962355d8580809cea3c68f55bbaaa746e64"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/boost_intrusive/4a7bf962355d8580809cea3c68f55bbaaa746e64'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"  clone --no-checkout --depth 1 --no-single-branch --config "advice.detachedHead=false" "https://github.com/boostorg/intrusive.git" "4a7bf962355d8580809cea3c68f55bbaaa746e64"
    WORKING_DIRECTORY "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/boost_intrusive"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/boostorg/intrusive.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git"  checkout boost-1.85.0 --
  WORKING_DIRECTORY "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/boost_intrusive/4a7bf962355d8580809cea3c68f55bbaaa746e64"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'boost-1.85.0'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git"  submodule update --recursive --init 
    WORKING_DIRECTORY "/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/boost_intrusive/4a7bf962355d8580809cea3c68f55bbaaa746e64"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/home/hiari/wd/metal-matmul/multi_core_reuse/.cpmcache/boost_intrusive/4a7bf962355d8580809cea3c68f55bbaaa746e64'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/boost_intrusive-subbuild/boost_intrusive-populate-prefix/src/boost_intrusive-populate-stamp/boost_intrusive-populate-gitinfo.txt"
    "/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/boost_intrusive-subbuild/boost_intrusive-populate-prefix/src/boost_intrusive-populate-stamp/boost_intrusive-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/home/hiari/wd/metal-matmul/multi_core_reuse/build/_deps/boost_intrusive-subbuild/boost_intrusive-populate-prefix/src/boost_intrusive-populate-stamp/boost_intrusive-populate-gitclone-lastrun.txt'")
endif()

