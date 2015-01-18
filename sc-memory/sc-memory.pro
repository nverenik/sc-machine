TEMPLATE = lib
TARGET = $$qtLibraryTarget(sc_memory)

DESTDIR = ../bin

OBJECTS_DIR = obj
MOC_DIR = moc

HEADERS += \
    src/sc_memory.h \
    src/sc-store/sc_link_helpers.h \
    src/sc-store/sc_fs_storage.h \
    src/sc-store/sc_element.h \
    src/sc-store/sc_defines.h \
    src/sc-store/sc_config.h \
    src/sc-store/sc_types.h \
    src/sc-store/sc_stream_private.h \
    src/sc-store/sc_stream_file.h \
    src/sc-store/sc_stream.h \
    src/sc-store/sc_store.h \
    src/sc-store/sc_storage.h \
    src/sc-store/sc_segment.h \
    src/sc-store/sc_stream_memory.h \
    src/sc-store/sc_event.h \
    src/sc-store/sc_iterator3.h \
    src/sc-store/sc_iterator5.h \
    src/sc-store/sc_iterator.h \
    src/sc_helper.h \
    src/sc_memory_headers.h \
    src/sc_memory_ext.h \
    src/sc-store/sc_fm_engine_private.h \
    src/sc-store/sc_fm_engine.h \
    src/sc_helper_private.h \
    src/sc_memory_private.h \
    src/sc_memory_version.h \
    src/sc-store/sc_event/sc_event_private.h \
    src/sc-store/sc_event/sc_event_queue.h \
    src/sc-store/sc_storage_snp/sc_storage_snp_glue.h \
    src/sc-store/sc_storage_snp/sc_storage_snp_types.h \
    src/sc-store/sc_storage_snp/sc_storage_snp_config.h

SOURCES += \
    src/sc_memory.c \
    src/sc-store/sc_link_helpers.c \
    src/sc-store/sc_fs_storage.c \
    src/sc-store/sc_element.c \
    src/sc-store/sc_stream_file.c \
    src/sc-store/sc_stream.c \
    src/sc-store/sc_storage.c \
    src/sc-store/sc_segment.c \
    src/sc-store/sc_stream_memory.c \
    src/sc-store/sc_event.c \
    src/sc-store/sc_iterator3.c \
    src/sc-store/sc_iterator5.c \
    src/sc_helper.c \
    src/sc_memory_ext.c \
    src/sc-store/sc_config.c \
    src/sc-store/sc_iterator.c \
    src/sc-store/sc_fm_engine.c \
    src/sc_memory_version.c \
    src/sc-store/sc_event/sc_event_queue.c \
    src/sc-store/sc_storage_snp.c \
    src/sc-store/sc_storage_snp/sc_storage_snp_glue.cpp

win32 {
    INCLUDEPATH += "../glib/include/glib-2.0"
    INCLUDEPATH += "../glib/lib/glib-2.0/include"

    POST_TARGETDEPS += ../glib/lib/glib-2.0.lib
    LIBS += ../glib/lib/glib-2.0.lib

    DEFINES += WIN32
}

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += glib-2.0
    PKGCONFIG += gmodule-2.0
}

# include this config to qmake to replace storage
# implementation with hardware (CUDA in the best case)
HardwareStorage {
    # TODO: move snp library inside of the project,
    # for now it's external
    SNPPATH = ../../snp
    DEFINES += ENABLE_HARDWARE_STORAGE
    QMAKE_CXXFLAGS += -std=c++11
    INCLUDEPATH += \
        $$SNPPATH/include

    SNPLIB = snp.rocksdb #snp.cuda
    CONFIG(debug, debug|release) {
        SNPLIB = $$join(SNPLIB,,,.debug)
    }
    #LIBS += $$quote(-L$$SNPPATH/prebuilt) -Wl,--whole-archive -l$$SNPLIB -Wl,--no-whole-archive
    LIBS += $$quote(-L$$SNPPATH/prebuilt) -l$$SNPLIB
}
