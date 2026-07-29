cmake_dependent_option(EUDAQ_BUILD_STDEVENT_MONITOR "monitor/StdEventMonitor executable (requires ROOT)" OFF
  "ROOT_FOUND" OFF)

if(NOT EUDAQ_BUILD_STDEVENT_MONITOR)
    message(STATUS "monitor/StdEventMonitor is NOT to be built (EUDAQ_BUILD_STDEVENT_MONITOR=OFF)")
  return()
endif()
message(STATUS "monitor/StdEventMonitor is to be built (EUDAQ_BUILD_STDEVENT_MONITOR=ON)")

find_package(ROOT REQUIRED COMPONENTS Core Gui Hist)
include("${ROOT_DIR}/RootMacros.cmake")

add_executable(StdEventMonitor
    src/BaseCollection.cc
    src/CorrelationCollection.cc
    src/CorrelationHistos.cc
    src/EUDAQMonitorCollection.cc
    src/EUDAQMonitorHistos.cc
    src/HitmapCollection.cc
    src/HitmapHistos.cc
    src/MonitorPerformanceCollection.cc
    src/MonitorPerformanceHistos.cc
    src/OnlineMon.cxx
    src/OnlineMonConfiguration.cc
    src/OnlineMonWindow.cc
    src/ParaMonitorCollection.cc
    src/SimpleStandardEvent.cc
    src/SimpleStandardPlane.cc
)

target_compile_definitions(StdEventMonitor PRIVATE
  $<$<PLATFORM_ID:Windows>:NOMINMAX;WIN32_LEAN_AND_MEAN>
)

target_link_libraries(StdEventMonitor
  PRIVATE
    ${EUDAQ_CORE_LIBRARY}
    ROOT::Core
    ROOT::Gui
    ROOT::Hist
)

set_target_properties(StdEventMonitor PROPERTIES ENABLE_EXPORTS true)
target_include_directories(StdEventMonitor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

root_generate_dictionary(
  G__StdEventMonitor
  include/OnlineMonWindow.hh
  MODULE StdEventMonitor
  LINKDEF include/StdEventMonitorLinkDef.h
)

install(TARGETS StdEventMonitor
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
  
