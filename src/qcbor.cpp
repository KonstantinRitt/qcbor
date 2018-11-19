#include "qcbor.h"

#define USE_CBOR_CONTEXT 1

#include <cn-cbor/cn-cbor.h>

static void *qcbor_encode_context_calloc(size_t count, size_t size, void *context)
{
    Q_UNUSED(context)

    return ::calloc(count, size);
}

static void qcbor_encode_context_free(void *ptr, void *context)
{
    Q_UNUSED(context)

    cn_cbor *obj = reinterpret_cast<cn_cbor *>(ptr);
    ::free(const_cast<char *>(obj->v.str));

    ::free(ptr);
}

static cn_cbor_context qcbor_encode_context = {
    &qcbor_encode_context_calloc,
    &qcbor_encode_context_free,
    nullptr
};

static cn_cbor_context qcbor_decode_context = {
    nullptr,
    nullptr,
    nullptr
};

static cn_cbor *cn_cbor_create(cn_cbor_type type CBOR_CONTEXT, cn_cbor_errback *errp)
{
    cn_cbor *obj = cn_cbor_data_create(nullptr, 0,
#ifdef USE_CBOR_CONTEXT
                                       context,
#endif
                                       errp);

    if (Q_LIKELY(obj != nullptr))
        obj->type = type;

    return obj;
}

namespace CBOR {

static QVariant cbor2qt(cn_cbor *obj, cn_cbor_errback *errp = nullptr)
{
    if (Q_UNLIKELY(obj == nullptr))
        return QVariant();

    switch (obj->type) {
    case CN_CBOR_FALSE:
    case CN_CBOR_TRUE:
        return QVariant::fromValue(obj->type == CN_CBOR_TRUE);
    case CN_CBOR_NULL:
        return QVariant(QMetaType::VoidStar, nullptr, true);
    case CN_CBOR_UNDEF:
        break;
    case CN_CBOR_UINT:
        return QVariant::fromValue(obj->v.uint);
    case CN_CBOR_INT:
        return QVariant::fromValue(obj->v.sint);
    case CN_CBOR_DOUBLE:
        return QVariant::fromValue(obj->v.dbl);
    case CN_CBOR_FLOAT:
        return QVariant::fromValue(obj->v.f);
    case CN_CBOR_BYTES:
        return QVariant::fromValue(QByteArray(obj->v.str, obj->length));
    case CN_CBOR_TEXT:
        return QVariant::fromValue(QString::fromUtf8(obj->v.str, obj->length));
    case CN_CBOR_BYTES_CHUNKED:
    case CN_CBOR_TEXT_CHUNKED:
        Q_UNREACHABLE();
        break;
    case CN_CBOR_ARRAY: {
        QVariantList list;
        list.reserve(obj->length);
        for (cn_cbor *child = obj->first_child; child != nullptr; child = child->next) {
            const QVariant v = cbor2qt(child, errp);
            if (Q_UNLIKELY(errp != nullptr && errp->err != CN_CBOR_NO_ERROR))
                return QVariant();

            list.append(v);
        }
        Q_ASSERT(list.size() == obj->length);
        return list;
    }
    case CN_CBOR_MAP: {
        QVariantMap map;
        //map.reserve(obj->length / 2);
        for (cn_cbor *child = obj->first_child; child != nullptr && child->next != nullptr; child = child->next->next) {
            const QString k = cbor2qt(child, errp).toString();
            if (Q_UNLIKELY(errp != nullptr && errp->err != CN_CBOR_NO_ERROR))
                return QVariant();

            const QVariant v = cbor2qt(child->next, errp);
            if (Q_UNLIKELY(errp != nullptr && errp->err != CN_CBOR_NO_ERROR))
                return QVariant();

            map.insertMulti(k, v);
        }
        Q_ASSERT(map.size() * 2 == obj->length);
        return map;
    }

    case CN_CBOR_SIMPLE:
    case CN_CBOR_TAG:
    case CN_CBOR_INVALID:
        Q_FALLTHROUGH();
    default:
        qFatal("cbor decode: unsupported cbor type %d", int(obj->type));
        break;
    }

    return QVariant();
}

static cn_cbor *qt2cbor(const QVariant &value, cn_cbor_errback *errp = nullptr)
{
    switch (value.userType()) {
    case QMetaType::UnknownType:
        return cn_cbor_create(CN_CBOR_UNDEF, &qcbor_encode_context, errp);

    case QMetaType::Void:
    case QMetaType::VoidStar:
    case QMetaType::QObjectStar:
        return cn_cbor_create(CN_CBOR_NULL, &qcbor_encode_context, errp);

    case QMetaType::Bool:
        return cn_cbor_create(value.toBool() ? CN_CBOR_TRUE : CN_CBOR_FALSE, &qcbor_encode_context, errp);

    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return cn_cbor_int_create(value.toLongLong(), &qcbor_encode_context, errp);

    case QMetaType::Float:
        return cn_cbor_float_create(value.toFloat(), &qcbor_encode_context, errp);

    case QMetaType::Double:
        return cn_cbor_double_create(value.toDouble(), &qcbor_encode_context, errp);

    case QMetaType::QChar: {
        const QByteArray data = QString(value.toChar()).toUtf8();
        char *buf = reinterpret_cast<char *>(::malloc(size_t(data.size())));
        ::memcpy(buf, data.constData(), size_t(data.size()));
        cn_cbor *obj = cn_cbor_data_create(reinterpret_cast<const uint8_t *>(buf), data.size(), &qcbor_encode_context, errp);
        obj->type = CN_CBOR_TEXT;
        return obj;
    }

    case QMetaType::QString: {
        const QByteArray data = value.toString().toUtf8();
        char *buf = reinterpret_cast<char *>(::malloc(size_t(data.size())));
        ::memcpy(buf, data.constData(), size_t(data.size()));
        cn_cbor *obj = cn_cbor_data_create(reinterpret_cast<const uint8_t *>(buf), data.size(), &qcbor_encode_context, errp);
        obj->type = CN_CBOR_TEXT;
        return obj;
    }
/* ### TODO
    case QMetaType::QBitArray: {
        const QByteArray data(value.toBitArray().data_ptr());
        char *buf = reinterpret_cast<char *>(::malloc(size_t(data.size())));
        ::memcpy(buf, data.constData(), size_t(data.size()));
        return cn_cbor_data_create(reinterpret_cast<const uint8_t *>(buf), data.size(), &qcbor_encode_context, errp);
    }
*/
    case QMetaType::QByteArray: {
        const QByteArray data = value.toByteArray();
        char *buf = reinterpret_cast<char *>(::malloc(size_t(data.size())));
        ::memcpy(buf, data.constData(), size_t(data.size()));
        return cn_cbor_data_create(reinterpret_cast<const uint8_t *>(buf), data.size(), &qcbor_encode_context, errp);
    }

    case QMetaType::QByteArrayList:
    case QMetaType::QStringList:
    case QMetaType::QVariantList: {
        cn_cbor *obj = cn_cbor_array_create(&qcbor_encode_context, errp);
        if (Q_LIKELY(obj != nullptr)) {
            const QList<QVariant> list = value.toList();
            for (const QVariant &v : list) {
                if (Q_UNLIKELY(!cn_cbor_array_append(obj, qt2cbor(v, errp), errp))) {
                    cn_cbor_free(obj, &qcbor_encode_context);
                    return nullptr;
                }
            }
            Q_ASSERT(list.size() == obj->length);
        }
        return obj;
    }

    case QMetaType::QVariantHash:
    case QMetaType::QVariantMap: {
        cn_cbor *obj = cn_cbor_map_create(&qcbor_encode_context, errp);
        if (Q_LIKELY(obj != nullptr)) {
            const QMap<QString, QVariant> map = value.toMap();
            for (auto it = map.cbegin(); it != map.cend(); ++it) {
                const QString &k = it.key();
                const QVariant &v = it.value();

                if (Q_UNLIKELY(!cn_cbor_map_put(obj, qt2cbor(k, errp), qt2cbor(v, errp), errp))) {
                    cn_cbor_free(obj, &qcbor_encode_context);
                    return nullptr;
                }
            }
            Q_ASSERT(map.size() * 2 == obj->length);
        }
        return obj;
    }
/* ### TODO
    case QMetaType::QDate:
    case QMetaType::QTime:
    case QMetaType::QDateTime:
    case QMetaType::QUrl:
    case QMetaType::QLocale:
    case QMetaType::QRect:
    case QMetaType::QRectF:
    case QMetaType::QSize:
    case QMetaType::QSizeF:
    case QMetaType::QLine:
    case QMetaType::QLineF:
    case QMetaType::QPoint:
    case QMetaType::QPointF:
    case QMetaType::QRegExp:
    case QMetaType::QRegularExpression:
    case QMetaType::QEasingCurve:
    case QMetaType::QUuid:
    case QMetaType::QVariant:
    case QMetaType::QModelIndex:
    case QMetaType::QPersistentModelIndex:
    case QMetaType::QJsonValue:
    case QMetaType::QJsonObject:
    case QMetaType::QJsonArray:
    case QMetaType::QJsonDocument:
*/
/* ### TODO
    case QMetaType::QFont:
    case QMetaType::QPixmap:
    case QMetaType::QBrush:
    case QMetaType::QColor:
    case QMetaType::QPalette:
    case QMetaType::QIcon:
    case QMetaType::QImage:
    case QMetaType::QPolygon:
    case QMetaType::QRegion:
    case QMetaType::QBitmap:
    case QMetaType::QCursor:
    case QMetaType::QKeySequence:
    case QMetaType::QPen:
    case QMetaType::QTextLength:
    case QMetaType::QTextFormat:
    case QMetaType::QMatrix:
    case QMetaType::QTransform:
    case QMetaType::QMatrix4x4:
    case QMetaType::QVector2D:
    case QMetaType::QVector3D:
    case QMetaType::QVector4D:
    case QMetaType::QQuaternion:
    case QMetaType::QPolygonF:
    case QMetaType::QSizePolicy:
    case QMetaType::User:
*/
    default:
        qFatal("cbor encode: unsupported variant type %d", value.userType());
        break;
    }

    return nullptr;
}

QVariant decode(const QByteArray &data)
{
    if (Q_UNLIKELY(data.isEmpty()))
        return QVariant();

    cn_cbor_errback err;
    err.err = CN_CBOR_NO_ERROR;
    err.pos = -1;

    cn_cbor *obj = cn_cbor_decode(reinterpret_cast<const uint8_t *>(data.constData()), size_t(data.size()), &qcbor_decode_context, &err);

    if (Q_UNLIKELY(err.err != CN_CBOR_NO_ERROR)) {
        qCritical("cbor decode error: %s at pos %d", cn_cbor_error_str[err.err], err.pos);

        Q_ASSERT(obj == nullptr);

        return QVariant();
    }

    Q_ASSERT(obj != nullptr);

    QVariant value = cbor2qt(obj, &err);

    if (Q_UNLIKELY(err.err != CN_CBOR_NO_ERROR))
        qCritical("cbor decode error: %s at pos %d", cn_cbor_error_str[err.err], err.pos);

    cn_cbor_free(obj, &qcbor_decode_context);

    return value;
}

QByteArray encode(const QVariant &value)
{
    cn_cbor_errback err;
    err.err = CN_CBOR_NO_ERROR;
    err.pos = -1;

    cn_cbor *obj = qt2cbor(value, &err);

    if (Q_UNLIKELY(err.err != CN_CBOR_NO_ERROR)) {
        qCritical("cbor encode error: %s at pos %d", cn_cbor_error_str[err.err], err.pos);

        if (obj != nullptr)
            cn_cbor_free(obj, &qcbor_encode_context);

        return QByteArray();
    }

    Q_ASSERT(obj != nullptr);

    QByteArray data(256, Qt::Uninitialized); // hopefully, 256 bytes is enough
    ssize_t len = cn_cbor_encoder_write(reinterpret_cast<uint8_t *>(data.data()), 0, size_t(data.size()), obj);

    if (Q_UNLIKELY(len < 0))
        err.err = CN_CBOR_ERR_OUT_OF_MEMORY;

    if (Q_UNLIKELY(err.err != CN_CBOR_NO_ERROR)) {
        qCritical("cbor encode error: %s at pos %d", cn_cbor_error_str[err.err], err.pos);

        cn_cbor_free(obj, &qcbor_encode_context);

        return QByteArray();
    }

    data.resize(len);

    cn_cbor_free(obj, &qcbor_encode_context);

    return data;
}

} // namespace CBOR
