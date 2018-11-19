qcbor
========
CBOR for Qt

Usage
------------
Clone
~~~bash
git submodule update --init
~~~

Include to your project
~~~qmake
include(qcbor/qcbor.pri)
~~~

Packing
~~~cpp
QList<int> list;
list << 1 << 2 << 3;
QByteArray array = CBOR::encode(QVariant::fromValue(list));
~~~

Unpacking:
~~~cpp
QVariant unpacked = CBOR::decode(data);
~~~

Thread-safety
-------------
Both `CBOR::encode` and `CBOR::decode` are safe to be used from multiple threads.
