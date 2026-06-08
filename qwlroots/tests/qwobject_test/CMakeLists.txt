set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
    Core
    Test
    REQUIRED
)

add_executable(test_qwobject
    wlr_abc.h
    wlr_abc.cpp
    qwabc.h
    test_qwobject.cpp
)

add_test(NAME QWObject COMMAND test_qwobject)

target_link_libraries(test_qwobject
PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
PRIVATE
    qwlroots
)

