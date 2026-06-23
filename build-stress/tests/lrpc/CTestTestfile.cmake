# CMake generated Testfile for 
# Source directory: /home/wangmaosen/app/cpp/KVStore/tests/lrpc
# Build directory: /home/wangmaosen/app/cpp/KVStore/build-stress/tests/lrpc
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_lrpc_protocol "/home/wangmaosen/app/cpp/KVStore/bin/test_lrpc_protocol")
set_tests_properties(test_lrpc_protocol PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/lrpc/CMakeLists.txt;1;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/lrpc/CMakeLists.txt;0;")
add_test(test_lrpc_boundary_fault "/home/wangmaosen/app/cpp/KVStore/bin/test_lrpc_boundary_fault")
set_tests_properties(test_lrpc_boundary_fault PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/home/wangmaosen/app/cpp/KVStore/tests/CMakeLists.txt;26;add_test;/home/wangmaosen/app/cpp/KVStore/tests/lrpc/CMakeLists.txt;2;kvstore_add_test;/home/wangmaosen/app/cpp/KVStore/tests/lrpc/CMakeLists.txt;0;")
