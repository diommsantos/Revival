include(CTest)

set (TestsToRun
    GetBothTimestepsTest.cpp
    GetOrderBookTimestepsTest.cpp
)

create_test_sourcelist (Tests CommonTests.cpp ${TestsToRun})

find_package(Qt6 REQUIRED COMPONENTS Core)

qt_add_executable(CommonTests 
  ${Tests}
)

target_link_libraries(CommonTests PRIVATE 
  Simulator
  Qt6::Core
  Qt6::Widgets
  Qt6::PrintSupport)

# Add all the ADD_TEST for each test
foreach (test ${TestsToRun})
  get_filename_component (TName ${test} NAME_WE)
  add_test (NAME ${TName} COMMAND CommonTests ${TName})
endforeach ()
