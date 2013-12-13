project(lxrandr-qt)

cmake_minimum_required(VERSION 2.8.6)

# Qt
find_package(Qt4 REQUIRED QtCore QtGui)


# glib using pkg-config
find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED
  glib-2.0
)

pkg_check_modules(X11 REQUIRED
  x11
)

pkg_check_modules(XRANDR REQUIRED
  xrandr
)

# set visibility to hidden to hide symbols, unlesss they're exporeted manually in the code
set(CMAKE_CXX_FLAGS "-DQT_NO_KEYWORDS -fno-exceptions")

include_directories(
  ${QT_INCLUDES}
  ${GLIB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${XRANDR_INCLUDE_DIRS}
  ${X11_INCLUDE_DIRS}
)

set(CMAKE_AUTOMOC TRUE)

set(lxrandr-qt_SRCS
  lxrandr-qt.cpp
  monitorsettingsdialog.cpp
  randr.cpp
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
  ${XRANDR_LIBRARIES}
  ${X11_LIBRARIES}
)

install(TARGETS lxrandr-qt RUNTIME DESTINATION bin)

# building tarball with CPack -------------------------------------------------
# To create a source distribution, type:
# make package_source
include (InstallRequiredSystemLibraries)
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set (CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")
set (CPACK_PACKAGE_VENDOR "")
set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "1")
set (CPACK_PACKAGE_VERSION_PATCH "0")
set (CPACK_GENERATOR TBZ2)
set (CPACK_SOURCE_GENERATOR TBZ2)
set (CPACK_SOURCE_IGNORE_FILES /build/;.gitignore;.*~;.git;.kdev4;temp)
include (CPack)
