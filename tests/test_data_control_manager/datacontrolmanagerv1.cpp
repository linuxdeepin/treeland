// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "datacontrolmanagerv1.h"

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define ZWLRDATACONTROLMANAGERV1VERSION 2

static const QString QtXImageLiteral QStringLiteral("application/x-qt-image");
static const QString utf8Text QStringLiteral("text/plain;charset=utf-8");
static const QString textPlain QStringLiteral("text/plain");

static QStringList imageMimeFormats(const QList<QByteArray> &imageFormats)
{
    QStringList formats;
    formats.reserve(imageFormats.size());
    for (const auto &format : imageFormats)
        formats.append(QLatin1String("image/") + QLatin1String(format.toLower()));

    int pngIndex = formats.indexOf(QLatin1String("image/png"));
    if (pngIndex != -1 && pngIndex != 0)
        formats.move(pngIndex, 0);
    return formats;
}

static inline QStringList imageReadMimeFormats()
{
    return imageMimeFormats(QImageReader::supportedImageFormats());
}

static inline QStringList imageWriteMimeFormats()
{
    return imageMimeFormats(QImageWriter::supportedImageFormats());
}

DataControlDeviceV1ManagerV1::DataControlDeviceV1ManagerV1()
    : QWaylandClientExtensionTemplate<DataControlDeviceV1ManagerV1>(ZWLRDATACONTROLMANAGERV1VERSION)
{
}

DataControlDeviceV1ManagerV1::~DataControlDeviceV1ManagerV1()
{
    if (isInitialized()) {
        destroy();
    }
}

void DataControlDeviceV1ManagerV1::instantiate()
{
    initialize();
}

DataControlOfferV1::DataControlOfferV1(struct ::zwlr_data_control_offer_v1 *id)
    : QtWayland::zwlr_data_control_offer_v1(id)
{
}

DataControlOfferV1::~DataControlOfferV1()
{
    destroy();
}

QStringList DataControlOfferV1::formats() const
{
    return m_receivedFormats;
}

bool DataControlOfferV1::containsImageData() const
{
    if (m_receivedFormats.contains(QtXImageLiteral)) {
        return true;
    }
    const auto formats = imageReadMimeFormats();
    for (const auto &receivedFormat : m_receivedFormats) {
        if (formats.contains(receivedFormat)) {
            return true;
        }
    }
    return false;
}

bool DataControlOfferV1::hasFormat(const QString &mimeType) const
{
    if (mimeType == textPlain && m_receivedFormats.contains(utf8Text)) {
        return true;
    }
    if (m_receivedFormats.contains(mimeType)) {
        return true;
    }

    if (containsImageData()) {
        const QStringList imageFormats = imageWriteMimeFormats();
        for (const QString &imageFormat : imageFormats) {
            if (imageFormat == mimeType) {
                return true;
            }
        }
        if (mimeType == QtXImageLiteral) {
            return true;
        }
    }

    return false;
}

void DataControlOfferV1::zwlr_data_control_offer_v1_offer(const QString &mime_type)
{
    m_receivedFormats << mime_type;

    QVariant data = retrieveData(mime_type);
    qWarning() << "------------------- offer changed---"
               << "type:=" << mime_type << "data:= " << data;
}

QVariant DataControlOfferV1::retrieveData(const QString &mimeType) const
{
    QString mime;
    if (!m_receivedFormats.contains(mimeType)) {
        if (mimeType == textPlain && m_receivedFormats.contains(utf8Text)) {
            mime = utf8Text;
        } else if (mimeType == QtXImageLiteral) {
            const auto writeFormats = imageWriteMimeFormats();
            for (const auto &receivedFormat : m_receivedFormats) {
                if (writeFormats.contains(receivedFormat)) {
                    mime = receivedFormat;
                    break;
                }
            }
            if (mime.isEmpty()) {
                mime = QStringLiteral("image/png");
            }
        }

        if (mime.isEmpty()) {
            return QVariant();
        }
    } else {
        mime = mimeType;
    }

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        return QVariant();
    }

    auto t = const_cast<DataControlOfferV1 *>(this);
    t->receive(mime, pipeFds[1]);

    close(pipeFds[1]);

    auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    auto display = waylandApp->display();

    wl_display_flush(display);

    QFile readPipe;
    if (readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
        QByteArray data;
        if (readData(pipeFds[0], data)) {
            close(pipeFds[0]);

            if (mimeType == QtXImageLiteral) {
                QImage img = QImage::fromData(
                    data,
                    mime.mid(mime.indexOf(QLatin1Char('/')) + 1).toLatin1().toUpper().data());
                if (!img.isNull()) {
                    return img;
                }
            }
            return data;
        }
        close(pipeFds[0]);
    }
    return QVariant();
}

bool DataControlOfferV1::readData(int fd, QByteArray &data)
{
    pollfd pfds[1];
    pfds[0].fd = fd;
    pfds[0].events = POLLIN;

    while (true) {
        const int ready = poll(pfds, 1, 1000);
        if (ready < 0) {
            if (errno != EINTR) {
                qWarning("DataControlOfferV1: poll() failed: %s", strerror(errno));
                return false;
            }
        } else if (ready == 0) {
            qWarning("DataControlOfferV1: timeout reading from pipe");
            return false;
        } else {
            char buf[4096];
            int n = read(fd, buf, sizeof buf);

            if (n < 0) {
                qWarning("DataControlOfferV1: read() failed: %s", strerror(errno));
                return false;
            } else if (n == 0) {
                return true;
            } else if (n > 0) {
                data.append(buf, n);
            }
        }
    }
}

DataControlSourceV1::DataControlSourceV1(struct ::zwlr_data_control_source_v1 *id,
                                         QMimeData *mimeData)
    : QtWayland::zwlr_data_control_source_v1(id)
    , m_mimeData(mimeData)
{
    const auto formats = mimeData->formats();
    for (const QString &format : formats) {
        offer(format);
    }
    if (mimeData->hasText()) {
        offer(utf8Text);
    }

    if (mimeData->hasImage()) {
        const QStringList imageFormats = imageWriteMimeFormats();
        for (const QString &imageFormat : imageFormats) {
            if (!formats.contains(imageFormat)) {
                offer(imageFormat);
            }
        }
    }
}

DataControlSourceV1::~DataControlSourceV1()
{
    destroy();
}

QMimeData *DataControlSourceV1::mimeData()
{
    return m_mimeData.get();
}

std::unique_ptr<QMimeData> DataControlSourceV1::releaseMimeData()
{
    return std::move(m_mimeData);
}

void DataControlSourceV1::zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd)
{
    QString send_mime_type = mime_type;
    if (send_mime_type == utf8Text) {
        send_mime_type = textPlain;
    }

    QByteArray ba;
    if (m_mimeData->hasImage()) {
        if (mime_type == QtXImageLiteral) {
            QImage image = qvariant_cast<QImage>(m_mimeData->imageData());
            QBuffer buf(&ba);
            buf.open(QBuffer::WriteOnly);
            image.save(&buf, "PNG");

        } else if (mime_type.startsWith(QLatin1String("image/"))) {
            QImage image = qvariant_cast<QImage>(m_mimeData->imageData());
            QBuffer buf(&ba);
            buf.open(QBuffer::WriteOnly);
            image.save(
                &buf,
                mime_type.mid(mime_type.indexOf(QLatin1Char('/')) + 1).toLatin1().toUpper().data());
        }
    } else {
        ba = m_mimeData->data(send_mime_type);
    }

    struct sigaction action, oldAction;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGPIPE, &action, &oldAction);
    write(fd, ba.constData(), ba.size());
    sigaction(SIGPIPE, &oldAction, nullptr);
    close(fd);
}

void DataControlSourceV1::zwlr_data_control_source_v1_cancelled()
{
    qWarning() << "-------------------cancelled";
    Q_EMIT cancelled();
}

DataControlDeviceV1::DataControlDeviceV1(struct ::zwlr_data_control_device_v1 *id)
    : QtWayland::zwlr_data_control_device_v1(id)
{
}

DataControlDeviceV1::~DataControlDeviceV1()
{
    destroy();
}

void DataControlDeviceV1::setSelection(std::unique_ptr<DataControlSourceV1> selection)
{
    m_selection = std::move(selection);
    connect(m_selection.get(), &DataControlSourceV1::cancelled, this, [this]() {
        m_selection.reset();
    });
    set_selection(m_selection->object());
    Q_EMIT selectionChanged();
}

QMimeData *DataControlDeviceV1::receivedSelection()
{
    return m_receivedSelection.get();
}

QMimeData *DataControlDeviceV1::selection()
{
    return m_selection ? m_selection->mimeData() : nullptr;
}

void DataControlDeviceV1::setPrimarySelection(std::unique_ptr<DataControlSourceV1> selection)
{
    m_primarySelection = std::move(selection);
    connect(m_primarySelection.get(), &DataControlSourceV1::cancelled, this, [this]() {
        m_primarySelection.reset();
    });

    if (zwlr_data_control_device_v1_get_version(object())
        >= ZWLR_DATA_CONTROL_DEVICE_V1_SET_PRIMARY_SELECTION_SINCE_VERSION) {
        set_primary_selection(m_primarySelection->object());
        Q_EMIT primarySelectionChanged();
    }
}

QMimeData *DataControlDeviceV1::receivedPrimarySelection()
{
    return m_receivedPrimarySelection.get();
}

QMimeData *DataControlDeviceV1::primarySelection()
{
    return m_primarySelection ? m_primarySelection->mimeData() : nullptr;
}

void DataControlDeviceV1::zwlr_data_control_device_v1_data_offer(zwlr_data_control_offer_v1 *id)
{
    new DataControlOfferV1(id);
}

void DataControlDeviceV1::zwlr_data_control_device_v1_selection(zwlr_data_control_offer_v1 *id)
{
    if (!id) {
        m_receivedSelection.reset();
    } else {
        auto derivated = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
        auto offer = dynamic_cast<DataControlOfferV1 *>(derivated);
        m_receivedSelection.reset(offer);
    }

    Q_EMIT receivedSelectionChanged();
}

void DataControlDeviceV1::zwlr_data_control_device_v1_primary_selection(
    zwlr_data_control_offer_v1 *id)
{
    if (!id) {
        m_receivedPrimarySelection.reset();
    } else {
        auto derivated = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
        auto offer = dynamic_cast<DataControlOfferV1 *>(derivated);
        m_receivedPrimarySelection.reset(offer);
    }
    Q_EMIT receivedPrimarySelectionChanged();
}
