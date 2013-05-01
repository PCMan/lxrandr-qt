project(lxrandr-qt)

cmake_minimum_required(VERSION 2.8.6)

# Qt
find_package(Qt4 REQUIRED QtCore QtGui)

# glib using pkg-config
find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED
  glib-2.0
)

# set visibility to hidden to hide symbols, unlesss they're exporeted manually in the code
set(CMAKE_CXX_FLAGS "-DQT_NO_KEYWORDS -fno-exceptions")

include_directories(
  ${QT_INCLUDES}
  ${GLIB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(CMAKE_AUTOMOC TRUE)

set(lxrandr-qt_SRCS
  lxrandr-qt.cpp
  monitorsettingsdialog.cpp
)

set(lxrandr-qt_UIS
  lxrandr.ui
  monitor.ui
)

qt4_wrap_ui(lxrandr-qt_UI_H
  ${lxrandr-qt_UIS}
)

add_executable(lxrandr-qt
  ${lxrandr-qt_SRCS}
  ${lxrandr-qt_UI_H}
)

target_link_libraries(lxrandr-qt
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${GLIB_LIBRARIES}
)

install(TARGETS lxrandr-qt RUNTIME DESTINATION bin)
