# Tests

add_executable(test_osp3_unit test_osp3_unit.c)
target_link_libraries(test_osp3_unit PRIVATE osp3)
add_test(test_osp3_unit test_osp3_unit)
