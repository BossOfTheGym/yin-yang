add_executable(test_component_storage test_component_storage.cpp)
target_link_libraries(test_component_storage PUBLIC yin_yang_lib)

add_executable(test_lofi_hashtable test_lofi_hashtable.cpp)
target_link_libraries(test_lofi_hashtable PUBLIC yin_yang_lib)

add_executable(test_yin_yang test_yin_yang.cpp)
target_link_libraries(test_yin_yang PUBLIC yin_yang_lib)

populate_filters()