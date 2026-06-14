# CMake generated Testfile for 
# Source directory: /home/wangmaosen/copy_linux/third/KVStore/ledis
# Build directory: /home/wangmaosen/copy_linux/third/KVStore/build-ledis/ledis
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(Ledis.protocol.test_resp_parser "/home/wangmaosen/copy_linux/third/KVStore/bin/Ledis/protocol/test_resp_parser")
set_tests_properties(Ledis.protocol.test_resp_parser PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;68;ledis_add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;0;")
add_test(Ledis.protocol.test_resp_encoder "/home/wangmaosen/copy_linux/third/KVStore/bin/Ledis/protocol/test_resp_encoder")
set_tests_properties(Ledis.protocol.test_resp_encoder PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;69;ledis_add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;0;")
add_test(Ledis.stream.test_ledis_stream "/home/wangmaosen/copy_linux/third/KVStore/bin/Ledis/stream/test_ledis_stream")
set_tests_properties(Ledis.stream.test_ledis_stream PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;70;ledis_add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;0;")
add_test(Ledis.session.test_session "/home/wangmaosen/copy_linux/third/KVStore/bin/Ledis/session/test_session")
set_tests_properties(Ledis.session.test_session PROPERTIES  _BACKTRACE_TRIPLES "/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;62;add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;71;ledis_add_test;/home/wangmaosen/copy_linux/third/KVStore/ledis/CMakeLists.txt;0;")
