file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_library (ui SHARED ${SOURCES} ${HEADERS})


include_directories("${SC_KPM_ROOT}/ui" ${SC_MEMORY_SRC} ${GLIB2_INCLUDE_DIRS})
add_dependencies(ui sc-memory)

install_targets("/lib/sc-memory/extensions" ui)
