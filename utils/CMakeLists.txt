# Utilities

add_executable(osp3-dump osp3-dump.c)
target_link_libraries(osp3-dump PRIVATE osp3)

add_executable(osp3-poll osp3-poll.c)
target_link_libraries(osp3-poll PRIVATE osp3)

install(TARGETS osp3-dump
                osp3-poll
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT OSP3_Utils_Runtime)
