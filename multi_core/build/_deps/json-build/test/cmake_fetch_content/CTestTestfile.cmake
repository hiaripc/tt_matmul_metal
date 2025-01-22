# CMake generated Testfile for 
# Source directory: /home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff/test/cmake_fetch_content
# Build directory: /home/hiari/wd/metal-matmul/multi_core/build/_deps/json-build/test/cmake_fetch_content
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(cmake_fetch_content_configure "/usr/bin/cmake" "-G" "Ninja" "-DCMAKE_CXX_COMPILER=/usr/bin/c++" "-Dnlohmann_json_source=/home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff" "/home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff/test/cmake_fetch_content/project")
set_tests_properties(cmake_fetch_content_configure PROPERTIES  FIXTURES_SETUP "cmake_fetch_content" LABELS "git_required" _BACKTRACE_TRIPLES "/home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff/test/cmake_fetch_content/CMakeLists.txt;2;add_test;/home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff/test/cmake_fetch_content/CMakeLists.txt;0;")
add_test(cmake_fetch_content_build "/usr/bin/cmake" "--build" ".")
set_tests_properties(cmake_fetch_content_build PROPERTIES  FIXTURES_REQUIRED "cmake_fetch_content" LABELS "git_required" _BACKTRACE_TRIPLES "/home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff/test/cmake_fetch_content/CMakeLists.txt;9;add_test;/home/hiari/wd/metal-matmul/multi_core/.cpmcache/json/230202b6f5267cbf0c8e5a2f17301964d95f83ff/test/cmake_fetch_content/CMakeLists.txt;0;")
