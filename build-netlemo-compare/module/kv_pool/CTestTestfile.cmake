# CMake generated Testfile for 
# Source directory: /home/wangmaosen/copy_linux/third/KVStore/module/kv_pool
# Build directory: /home/wangmaosen/copy_linux/third/KVStore/build-netlemo-compare/module/kv_pool
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(kv_pool.test_pool_mt "/home/wangmaosen/copy_linux/third/KVStore/bin/kv_pool/test_pool_mt")
set_tests_properties(kv_pool.test_pool_mt PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/cmake/KVStoreHelpers.cmake;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/module/kv_pool/CMakeLists.txt;10;kvstore_add_tests;/home/wangmaosen/copy_linux/third/KVStore/module/kv_pool/CMakeLists.txt;0;")
add_test(kv_pool.test_cross_thread "/home/wangmaosen/copy_linux/third/KVStore/bin/kv_pool/test_cross_thread")
set_tests_properties(kv_pool.test_cross_thread PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/cmake/KVStoreHelpers.cmake;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/module/kv_pool/CMakeLists.txt;10;kvstore_add_tests;/home/wangmaosen/copy_linux/third/KVStore/module/kv_pool/CMakeLists.txt;0;")
