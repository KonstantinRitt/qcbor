#ifndef QCBOR_H
#define QCBOR_H

#include <QtCore/qvariant.h>

namespace CBOR {

QVariant decode(const QByteArray &data);
QByteArray encode(const QVariant &value);

} // namespace CBOR

#endif // QCBOR_H
