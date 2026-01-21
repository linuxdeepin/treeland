// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "mpvvideocontroller.h"
#include "loggings.h"

MpvVideoController::MpvVideoController(QObject *parent)
    : QObject(parent)
{
}

QStringView MpvVideoController::getError(int error)
{
    switch (error) {
    case MPV_ERROR_SUCCESS:
        return u"MPV_ERROR_SUCCESS";
    case MPV_ERROR_EVENT_QUEUE_FULL:
        return u"MPV_ERROR_EVENT_QUEUE_FULL";
    case MPV_ERROR_NOMEM:
        return u"MPV_ERROR_NOMEM";
    case MPV_ERROR_UNINITIALIZED:
        return u"MPV_ERROR_UNINITIALIZED";
    case MPV_ERROR_INVALID_PARAMETER:
        return u"MPV_ERROR_INVALID_PARAMETER";
    case MPV_ERROR_OPTION_NOT_FOUND:
        return u"MPV_ERROR_OPTION_NOT_FOUND";
    case MPV_ERROR_OPTION_FORMAT:
        return u"MPV_ERROR_OPTION_FORMAT";
    case MPV_ERROR_OPTION_ERROR:
        return u"MPV_ERROR_OPTION_ERROR";
    case MPV_ERROR_PROPERTY_NOT_FOUND:
        return u"MPV_ERROR_PROPERTY_NOT_FOUND";
    case MPV_ERROR_PROPERTY_FORMAT:
        return u"MPV_ERROR_PROPERTY_FORMAT";
    case MPV_ERROR_PROPERTY_UNAVAILABLE:
        return u"MPV_ERROR_PROPERTY_UNAVAILABLE";
    case MPV_ERROR_PROPERTY_ERROR:
        return u"MPV_ERROR_PROPERTY_ERROR";
    case MPV_ERROR_COMMAND:
        return u"MPV_ERROR_COMMAND";
    case MPV_ERROR_LOADING_FAILED:
        return u"MPV_ERROR_LOADING_FAILED";
    case MPV_ERROR_AO_INIT_FAILED:
        return u"MPV_ERROR_AO_INIT_FAILED";
    case MPV_ERROR_VO_INIT_FAILED:
        return u"MPV_ERROR_VO_INIT_FAILED";
    case MPV_ERROR_NOTHING_TO_PLAY:
        return u"MPV_ERROR_NOTHING_TO_PLAY";
    case MPV_ERROR_UNKNOWN_FORMAT:
        return u"MPV_ERROR_UNKNOWN_FORMAT";
    case MPV_ERROR_UNSUPPORTED:
        return u"MPV_ERROR_UNSUPPORTED";
    case MPV_ERROR_NOT_IMPLEMENTED:
        return u"MPV_ERROR_NOT_IMPLEMENTED";
    case MPV_ERROR_GENERIC:
        return u"MPV_ERROR_GENERIC";
    }
    return {};
}


void MpvVideoController::mpvEvents(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvVideoController *>(ctx),
                              &MpvVideoController::eventHandler,
                              Qt::QueuedConnection);
}

void MpvVideoController::eventHandler()
{
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
        case MPV_EVENT_START_FILE: {
            Q_EMIT fileStarted();
            break;
        }

        case MPV_EVENT_FILE_LOADED: {
            Q_EMIT fileLoaded();
            break;
        }

        case MPV_EVENT_END_FILE: {
            auto prop = static_cast<mpv_event_end_file *>(event->data);
            if (prop->reason == MPV_END_FILE_REASON_EOF) {
                Q_EMIT endFile(QByteArray("eof"));
            } else if (prop->reason == MPV_END_FILE_REASON_ERROR) {
                Q_EMIT endFile(QByteArray("error"));
            }
            break;
        }

        case MPV_EVENT_VIDEO_RECONFIG: {
            Q_EMIT videoReconfig();
            break;
        }

        case MPV_EVENT_GET_PROPERTY_REPLY: {
            mpv_event_property *prop = static_cast<mpv_event_property *>(event->data);
            auto data = nodeToVariant(reinterpret_cast<mpv_node *>(prop->data));
            Q_EMIT asyncReply(data, {*event});
            break;
        }

        case MPV_EVENT_SET_PROPERTY_REPLY: {
            Q_EMIT asyncReply(QVariant(), {*event});
            break;
        }

        case MPV_EVENT_COMMAND_REPLY: {
            mpv_event_property *prop = static_cast<mpv_event_property *>(event->data);
            auto data = nodeToVariant(reinterpret_cast<mpv_node *>(prop));
            Q_EMIT asyncReply(data, {*event});
            break;
        }

        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop = static_cast<mpv_event_property *>(event->data);
            QVariant data;
            switch (prop->format) {
            case MPV_FORMAT_DOUBLE:
                data = *reinterpret_cast<double *>(prop->data);
                break;
            case MPV_FORMAT_STRING:
                data = QString::fromStdString(*reinterpret_cast<char **>(prop->data));
                break;
            case MPV_FORMAT_INT64:
                data = qlonglong(*reinterpret_cast<int64_t *>(prop->data));
                break;
            case MPV_FORMAT_FLAG:
                data = *reinterpret_cast<bool *>(prop->data);
                break;
            case MPV_FORMAT_NODE:
                data = nodeToVariant(reinterpret_cast<mpv_node *>(prop->data));
                break;
            case MPV_FORMAT_NONE:
            case MPV_FORMAT_OSD_STRING:
            case MPV_FORMAT_NODE_ARRAY:
            case MPV_FORMAT_NODE_MAP:
            case MPV_FORMAT_BYTE_ARRAY:
                break;
            }
            Q_EMIT propertyChanged(QByteArray(prop->name), data);
            break;
        }
        case MPV_EVENT_NONE:
        case MPV_EVENT_SHUTDOWN:
        case MPV_EVENT_LOG_MESSAGE:
        case MPV_EVENT_CLIENT_MESSAGE:
        case MPV_EVENT_AUDIO_RECONFIG:
        case MPV_EVENT_SEEK:
        case MPV_EVENT_PLAYBACK_RESTART:
        case MPV_EVENT_QUEUE_OVERFLOW:
        case MPV_EVENT_HOOK:
#if MPV_ENABLE_DEPRECATED
        case MPV_EVENT_IDLE:
        case MPV_EVENT_TICK:
#endif
            break;
        }
    }
}

mpv_handle *MpvVideoController::mpv() const
{
    return m_mpv;
}

void MpvVideoController::init()
{
    std::setlocale(LC_NUMERIC, "C");
    m_mpv = mpv_create();
    if (!m_mpv) {
        qCFatal(WALLPAPER) << "could not create mpv context";
    }

    mpv_set_option_string(m_mpv, "hwdec", "auto");

    if (mpv_initialize(m_mpv) < 0) {
        qCFatal(WALLPAPER) << "could not initialize mpv context";
    }

    mpv_set_wakeup_callback(m_mpv, MpvVideoController::mpvEvents, this);
    // use number of logical CPU cores
    setProperty(QByteArrayView("ad-lavc-threads"), 0);
    // render to fbo
    setProperty(QByteArrayView("vo"), QStringLiteral("libmpv"));
}

void MpvVideoController::observeProperty(const QByteArrayView &property, mpv_format format, uint64_t id)
{
    mpv_observe_property(mpv(), id, property.constData(), format);
}

int MpvVideoController::unobserveProperty(uint64_t id)
{
    return mpv_unobserve_property(mpv(), id);
}

int MpvVideoController::setProperty(const QByteArrayView &property, const QVariant &value)
{
    mpv_node node;
    setNode(&node, value);
    return mpv_set_property(m_mpv, property.constData(), MPV_FORMAT_NODE, &node);
}

int MpvVideoController::setPropertyAsync(const QByteArrayView &property, const QVariant &value, int id)
{
    mpv_node node;
    setNode(&node, value);
    return mpv_set_property_async(m_mpv, id, property.constData(), MPV_FORMAT_NODE, &node);
}

QVariant MpvVideoController::getProperty(const QByteArrayView &property)
{
    mpv_node node;
    int err = mpv_get_property(m_mpv, property.constData(), MPV_FORMAT_NODE, &node);
    if (err < 0) {
        return QVariant::fromValue(err);
    }

    QVariant r = nodeToVariant(&node);
    mpv_free_node_contents(&node);
    return r;
}

int MpvVideoController::getPropertyAsync(const QByteArrayView &property, int id)
{
    return mpv_get_property_async(m_mpv, id, property.constData(), MPV_FORMAT_NODE);
}

QVariant MpvVideoController::command(const QVariant &params)
{
    mpv_node node;
    setNode(&node, params);
    mpv_node result;
    int err = mpv_command_node(m_mpv, &node, &result);
    if (err < 0) {
        qCCritical(WALLPAPER) << getError(err) << params;
        return QVariant::fromValue(err);
    }

    QVariant r = nodeToVariant(&result);
    mpv_free_node_contents(&result);
    return r;
}

int MpvVideoController::commandAsync(const QVariant &params, int id)
{
    mpv_node node;
    setNode(&node, params);
    return mpv_command_node_async(m_mpv, id, &node);
}

mpv_node_list *MpvVideoController::createList(mpv_node *dst, bool is_map, int num)
{
    dst->format = is_map ? MPV_FORMAT_NODE_MAP : MPV_FORMAT_NODE_ARRAY;
    mpv_node_list *list = new mpv_node_list();
    dst->u.list = list;
    if (!list) {
        freeNode(dst);
        return nullptr;
    }
    list->values = new mpv_node[num]();
    if (!list->values) {
        freeNode(dst);
        return nullptr;
    }
    if (is_map) {
        list->keys = new char *[num]();
        if (!list->keys) {
            freeNode(dst);
            return nullptr;
        }
    }

    return list;
}

void MpvVideoController::setNode(mpv_node *dst, const QVariant &src)
{
    if (src.typeId() == QMetaType::QString) {
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = qstrdup(src.toString().toUtf8().data());
        if (!dst->u.string) {
            dst->format = MPV_FORMAT_NONE;
        }
    } else if (src.typeId() == QMetaType::Bool) {
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = src.toBool() ? 1 : 0;
    } else if (src.typeId() == QMetaType::Int ||
               src.typeId() == QMetaType::LongLong ||
               src.typeId() == QMetaType::UInt ||
               src.typeId() == QMetaType::ULongLong) {
        dst->format = MPV_FORMAT_INT64;
        dst->u.int64 = src.toLongLong();
    } else if (src.typeId() == QMetaType::Double) {
        dst->format = MPV_FORMAT_DOUBLE;
        dst->u.double_ = src.toDouble();
    } else if (src.canConvert<QVariantList>()) {
        QVariantList qlist = src.toList();
        mpv_node_list *list = createList(dst, false, qlist.size());
        if (!list) {
            dst->format = MPV_FORMAT_NONE;
            return;
        }
        list->num = qlist.size();
        for (int n = 0; n < qlist.size(); ++n) {
            setNode(&list->values[n], qlist[n]);
        }
    } else if (src.canConvert<QVariantMap>()) {
        QVariantMap qmap = src.toMap();
        mpv_node_list *list = createList(dst, true, qmap.size());
        if (!list) {
            dst->format = MPV_FORMAT_NONE;
            return;
        }
        list->num = qmap.size();
        int n = 0;
        for (auto it = qmap.constKeyValueBegin(); it != qmap.constKeyValueEnd(); ++it) {
            list->keys[n] = qstrdup(it.operator*().first.toUtf8().data());
            if (!list->keys[n]) {
                freeNode(dst);
                dst->format = MPV_FORMAT_NONE;
                return;
            }
            setNode(&list->values[n], it.operator*().second);
            ++n;
        }
    } else {
        dst->format = MPV_FORMAT_NONE;
    }

    return;
}

void MpvVideoController::freeNode(mpv_node *dst)
{
    switch (dst->format) {
    case MPV_FORMAT_STRING:
        delete[] dst->u.string;
        break;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP: {
        mpv_node_list *list = dst->u.list;
        if (list) {
            for (int n = 0; n < list->num; ++n) {
                if (list->keys) {
                    delete[] list->keys[n];
                }
                if (list->values) {
                    freeNode(&list->values[n]);
                }
            }
            delete[] list->keys;
            delete[] list->values;
        }
        delete list;
        break;
    }
    default:;
    }
    dst->format = MPV_FORMAT_NONE;
}

QVariant MpvVideoController::nodeToVariant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
        return QVariant(QString::fromUtf8(node->u.string));
    case MPV_FORMAT_FLAG:
        return QVariant(static_cast<bool>(node->u.flag));
    case MPV_FORMAT_INT64:
        return QVariant(static_cast<qlonglong>(node->u.int64));
    case MPV_FORMAT_DOUBLE:
        return QVariant(node->u.double_);
    case MPV_FORMAT_NODE_ARRAY: {
        mpv_node_list *list = node->u.list;
        QVariantList qlist;
        for (int n = 0; n < list->num; ++n) {
            qlist.append(nodeToVariant(&list->values[n]));
        }
        return QVariant(qlist);
    }
    case MPV_FORMAT_NODE_MAP: {
        mpv_node_list *list = node->u.list;
        QVariantMap qmap;
        for (int n = 0; n < list->num; ++n) {
            qmap.insert(QString::fromUtf8(list->keys[n]), nodeToVariant(&list->values[n]));
        }
        return QVariant(qmap);
    }
    default:
        return QVariant();
    }
}
