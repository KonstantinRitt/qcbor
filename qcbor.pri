INCLUDEPATH += $$PWD/include

HEADERS += \
    $$PWD/src/qcbor.h

SOURCES += \
    $$PWD/src/qcbor.cpp


CN_CBOR_PATH = $$PWD/cn-cbor

#DEFINES += \
#    CBOR_ALIGN_READS

INCLUDEPATH += $$CN_CBOR_PATH/include

HEADERS += \
    $$CN_CBOR_PATH/include/cn-cbor/cn-cbor.h

SOURCES += \
    $$CN_CBOR_PATH/src/cn-cbor.c \
    $$CN_CBOR_PATH/src/cn-create.c \
    $$CN_CBOR_PATH/src/cn-encoder.c \
    $$CN_CBOR_PATH/src/cn-error.c \
    $$CN_CBOR_PATH/src/cn-get.c
