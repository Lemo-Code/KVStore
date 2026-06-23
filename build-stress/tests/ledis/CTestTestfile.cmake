# CMake generated Testfile for 
# Source directory: /home/wangmaosen/app/cpp/KVStore/tests/ledis
# Build directory: /home/wangmaosen/app/cpp/KVStore/build-stress/tests/ledis
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_ledis_resp "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_resp")
set_tests_properties(test_ledis_resp PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;3;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_core "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_core")
set_tests_properties(test_ledis_core PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;4;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_storage "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_storage")
set_tests_properties(test_ledis_storage PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;5;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_storage_extended "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_storage_extended")
set_tests_properties(test_ledis_storage_extended PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;6;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_storage_smoke "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_storage_smoke")
set_tests_properties(test_ledis_storage_smoke PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;7;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_coverage_boost "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_coverage_boost")
set_tests_properties(test_ledis_coverage_boost PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;8;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_cluster "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_cluster")
set_tests_properties(test_ledis_cluster PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;9;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_cluster_manager "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_cluster_manager")
set_tests_properties(test_ledis_cluster_manager PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;10;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_aof "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_aof")
set_tests_properties(test_ledis_aof PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;11;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_lua "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_lua")
set_tests_properties(test_ledis_lua PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;12;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
add_test(test_ledis_boundary_fault "/home/wangmaosen/app/cpp/KVStore/bin/test_ledis_boundary_fault")
set_tests_properties(test_ledis_boundary_fault PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;13;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/ledis/CMakeLists.txt;0;")
