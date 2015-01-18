TEMPLATE = subdirs

CONFIG += ordered
DESTDIR = ./bin

SUBDIRS = sc-memory \
          sc-fm \
          sc-memory/test #\
          #tools/sc-store-visual

# include this config to qmake to replace storage
# implementation with hardware (CUDA in the best case)
HardwareStorage {
    DEFINES += ENABLE_HARDWARE_STORAGE
}
