# serialkit_headless: pure QtCore + QtSerialPort logic, no QWidget
# dependency, so it can be unit-tested without a display (see
# docs/ARCHITECTURE.md "Testing strategy"). QCustomPlot (GPL) must never be
# linked into this target -- see CLAUDE.md.
add_library(serialkit_headless STATIC
    transport/itransport.h
    transport/serial_transport.h
    transport/serial_transport.cpp
    core/ring_buffer.h
    core/ring_buffer.cpp
    core/frame_strategy.h
    core/delimiter_frame_strategy.h
    core/delimiter_frame_strategy.cpp
    core/passthrough_frame_strategy.h
    core/passthrough_frame_strategy.cpp
    core/raw_logger.h
    core/raw_logger.cpp
    core/session.h
    core/session.cpp
    core/session_manager.h
    core/session_manager.cpp
    plugin/api/iprotocol_decoder.h
    plugin/api/raw_passthrough_decoder.h
    plugin/api/raw_passthrough_decoder.cpp
    plugin/api/iplot_view.h
    send/send_encoding.h
    send/send_encoding.cpp
)
target_include_directories(serialkit_headless PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(serialkit_headless PUBLIC Qt6::Core Qt6::SerialPort)

# serialkit_ui: QWidget-based views and the main window. Depends on
# serialkit_headless for Session/RingBuffer/etc.
#
# terminal/vterm_engine.{h,cpp} doesn't itself need QWidget (it's a QObject
# wrapper around the libvterm C API), but it has exactly one consumer
# (terminal_view.cpp) and isn't unit-tested headlessly for M2, so it lives
# here rather than justifying a third library tier -- see
# docs/ARCHITECTURE.md M2 section.
add_library(serialkit_ui STATIC
    view/app_style.h
    view/app_style.cpp
    view/hex_view.h
    view/hex_view.cpp
    view/terminal_view.h
    view/terminal_view.cpp
    terminal/vterm_engine.h
    terminal/vterm_engine.cpp
    send/multi_line_send.h
    send/multi_line_send.cpp
    send/macro_panel.h
    send/macro_panel.cpp
    view/session_panel.h
    view/session_panel.cpp
    view/main_window.h
    view/main_window.cpp
)
target_link_libraries(serialkit_ui PUBLIC serialkit_headless Qt6::Widgets vterm_c)
# Single source of truth for the version shown in the window title
# (MainWindow) is PROJECT_VERSION, set once in the root CMakeLists.txt --
# not duplicated as a separate literal here.
target_compile_definitions(serialkit_ui PRIVATE SERIALKIT_VERSION="${PROJECT_VERSION}")

set(SERIALKIT_APP_SOURCES main.cpp)

if(APPLE)
  set(SERIALKIT_ICON_NAME serialkit.icns)
  set(SERIALKIT_ICON_FILE ${CMAKE_SOURCE_DIR}/resources/${SERIALKIT_ICON_NAME})
  set_source_files_properties(${SERIALKIT_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  list(APPEND SERIALKIT_APP_SOURCES ${SERIALKIT_ICON_FILE})
endif()

add_executable(serialkit_app ${SERIALKIT_APP_SOURCES})
target_link_libraries(serialkit_app PRIVATE serialkit_ui)

if(APPLE)
  # CFBundleIdentifier/BundleName/Version were empty without these --
  # CMake's built-in Info.plist.in template only fills them in from these
  # exact target properties. An empty CFBundleIdentifier is why the Dock/
  # Finder icon wasn't showing: LaunchServices keys its icon registration
  # off the bundle identifier, so a blank one leaves the icon unassociated
  # even though the .icns file itself is present and valid.
  # OUTPUT_NAME/GUI_IDENTIFIER stay lowercase "serialkit" on purpose: these
  # are internal identifiers, and QApplication's applicationName/
  # organizationName (main.cpp) -- which is what QSettings actually keys
  # off of, not CFBundleIdentifier -- are also still "serialkit". Only the
  # user-visible display name (CFBundleName, shown in Finder/Dock/menu bar)
  # changes to "SerialKit". This keeps existing saved Macro settings intact.
  set_target_properties(serialkit_app PROPERTIES
    OUTPUT_NAME "serialkit"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE ${SERIALKIT_ICON_NAME}
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.serialkit.app"
    MACOSX_BUNDLE_BUNDLE_NAME "SerialKit"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "SerialKit"
  )
endif()
