if (NOT BMALLOC_DIR)
    set(BMALLOC_DIR "${CMAKE_SOURCE_DIR}/Source/bmalloc")
endif ()
if (NOT WTF_DIR)
    set(WTF_DIR "${CMAKE_SOURCE_DIR}/Source/WTF")
endif ()
if (NOT JAVASCRIPTCORE_DIR)
    set(JAVASCRIPTCORE_DIR "${CMAKE_SOURCE_DIR}/Source/JavaScriptCore")
endif ()
if (NOT WEBCORE_DIR)
    set(WEBCORE_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore")
endif ()
if (NOT WEBKIT_DIR)
    set(WEBKIT_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit")
endif ()
if (NOT WEBKIT2_DIR)
    set(WEBKIT2_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit2")
endif ()
if (NOT THIRDPARTY_DIR)
    set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/Source/ThirdParty")
endif ()
if (NOT TOOLS_DIR)
    set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/Tools")
endif ()

set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(DERIVED_SOURCES_JAVASCRIPTCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore")
set(DERIVED_SOURCES_WEBCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebCore")
set(DERIVED_SOURCES_WEBKITLEGACY_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitLegacy")
set(DERIVED_SOURCES_WEBKIT_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit")
set(DERIVED_SOURCES_WEBKIT2_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit2")
set(DERIVED_SOURCES_WEBINSPECTORUI_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebInspectorUI")
set(DERIVED_SOURCES_WTF_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WTF")

file(MAKE_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WTF_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Source/JavaScriptCore/runtime)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/Protocol)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol)

if (ENABLE_WEBKIT2)
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})
endif ()

if (ENABLE_WEBKIT)
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR})
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT_DIR})
endif ()
