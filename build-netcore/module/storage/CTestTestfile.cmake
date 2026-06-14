# CMake generated Testfile for 
# Source directory: /home/wangmaosen/copy_linux/third/KVStore/module/storage
# Build directory: /home/wangmaosen/copy_linux/third/KVStore/build-netcore/module/storage
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(storage.lsmtree.test_lsm_tree "/home/wangmaosen/copy_linux/third/KVStore/bin/storage/lsmtree/test_lsm_tree")
set_tests_properties(storage.lsmtree.test_lsm_tree PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/cmake/KVStoreHelpers.cmake;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/module/storage/CMakeLists.txt;11;kvstore_add_tests;/home/wangmaosen/copy_linux/third/KVStore/module/storage/CMakeLists.txt;0;")
add_test(storage.lsmtree.test_lsm_persistent "/home/wangmaosen/copy_linux/third/KVStore/bin/storage/lsmtree/test_lsm_persistent")
set_tests_properties(storage.lsmtree.test_lsm_persistent PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/cmake/KVStoreHelpers.cmake;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/module/storage/CMakeLists.txt;11;kvstore_add_tests;/home/wangmaosen/copy_linux/third/KVStore/module/storage/CMakeLists.txt;0;")
