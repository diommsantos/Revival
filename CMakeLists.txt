cmake_minimum_required(VERSION 3.16)

project(Simulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.0/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS 
    Core
    Widgets
    PrintSupport
)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Simulator
    main.cpp
    Simulator.cpp
    SimulatorUI.cpp
    qcustomplot.cpp
)

target_link_libraries(Simulator PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::PrintSupport
)

if(MSVC)
    add_custom_command(TARGET Simulator POST_BUILD 
                       COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-compiler-runtime --no-translations --no-opengl-sw --force $<TARGET_FILE:Simulator> --list relative VERBATIM)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/PROFILE")
endif()

enable_testing()
add_subdirectory(tests)