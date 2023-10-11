// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputviewport.h"
#include "woutputviewport_p.h"
#include "woutput.h"

#include <qwbuffer.h>
#include <qwswapchain.h>

#include <QDebug>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class OutputTextureProvider : public QSGTextureProvider
{
public:
    explicit OutputTextureProvider(WOutputViewport *viewport)
        : item(viewport) {}
    ~OutputTextureProvider() {

    }

    QSGTexture *texture() const override {
        return dwtexture ? dwtexture->getSGTexture(item->window()) : nullptr;
    }

    void setBuffer(QWBuffer *buffer) {
        qwtexture.reset();

        if (Q_LIKELY(buffer)) {
            qwtexture.reset(QWTexture::fromBuffer(item->output()->renderer(), buffer));
        }

        if (Q_LIKELY(qwtexture)) {
            if (Q_LIKELY(dwtexture)) {
                dwtexture->setHandle(qwtexture.get());
            } else {
                dwtexture.reset(new WTexture(qwtexture.get()));
            }
        } else {
            dwtexture.reset();
        }

        Q_EMIT textureChanged();
    }

    WOutputViewport *item;
    std::unique_ptr<QWTexture> qwtexture;
    std::unique_ptr<WTexture> dwtexture;
};

void WOutputViewportPrivate::initForOutput()
{
    W_Q(WOutputViewport);

    outputWindow()->attach(q);

    QObject::connect(output, &WOutput::modeChanged, q, [this] {
        updateImplicitSize();
    });

    updateImplicitSize();
}

void WOutputViewportPrivate::invalidateSceneGraph()
{
    if (textureProvider)
        textureProvider.reset();
}

qreal WOutputViewportPrivate::getImplicitWidth() const
{
    return output->size().width() / devicePixelRatio;
}

qreal WOutputViewportPrivate::getImplicitHeight() const
{
    return output->size().height() / devicePixelRatio;
}

void WOutputViewportPrivate::updateImplicitSize()
{
    implicitWidthChanged();
    implicitHeightChanged();

    W_Q(WOutputViewport);
    q->resetWidth();
    q->resetHeight();
}

WOutputViewport::WOutputViewport(QQuickItem *parent)
    : QQuickItem(*new WOutputViewportPrivate(), parent)
{

}

WOutputViewport::~WOutputViewport()
{
    invalidate();
}

void WOutputViewport::invalidate()
{
    W_D(WOutputViewport);
    if (d->componentComplete && d->output && d->window) {
        d->outputWindow()->detach(this);
        d->output = nullptr;
    }
}

bool WOutputViewport::isTextureProvider() const
{
    W_DC(WOutputViewport);
    return d->textureProvider ? true : QQuickItem::isTextureProvider();
}

QSGTextureProvider *WOutputViewport::textureProvider() const
{
    W_DC(WOutputViewport);
    return d->textureProvider ? d->textureProvider.get() : QQuickItem::textureProvider();
}

WOutput *WOutputViewport::output() const
{
    W_D(const WOutputViewport);
    return d->output;
}

void WOutputViewport::setOutput(WOutput *newOutput)
{
    W_D(WOutputViewport);

    Q_ASSERT(!d->output || !newOutput);
    d->output = newOutput;

    if (d->componentComplete) {
        if (newOutput)
            d->initForOutput();
    }
}

void WOutputViewport::setBuffer(QWBuffer *buffer)
{
    W_D(WOutputViewport);
    if (d->textureProvider)
        d->textureProvider->setBuffer(buffer);
}

qreal WOutputViewport::devicePixelRatio() const
{
    W_DC(WOutputViewport);
    return d->devicePixelRatio;
}

void WOutputViewport::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputViewport);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

bool WOutputViewport::offscreen() const
{
    W_DC(WOutputViewport);
    return d->offscreen;
}

void WOutputViewport::setOffscreen(bool newOffscreen)
{
    W_D(WOutputViewport);
    if (d->offscreen == newOffscreen)
        return;
    d->offscreen = newOffscreen;
    Q_EMIT offscreenChanged();
}

bool WOutputViewport::isRoot() const
{
    W_DC(WOutputViewport);
    return d->root;
}

void WOutputViewport::setRoot(bool newRoot)
{
    W_D(WOutputViewport);
    if (d->root == newRoot)
        return;
    d->root = newRoot;

    if (newRoot) {
        d->refFromEffectItem(true);
    } else {
        d->derefFromEffectItem(true);
    }

    Q_EMIT rootChanged();
}

bool WOutputViewport::cacheBuffer() const
{
    W_DC(WOutputViewport);
    return d->cacheBuffer;
}

void WOutputViewport::setCacheBuffer(bool newCacheBuffer)
{
    W_D(WOutputViewport);
    if (d->cacheBuffer == newCacheBuffer)
        return;
    d->cacheBuffer = newCacheBuffer;

    if (d->cacheBuffer) {
        Q_ASSERT(!d->textureProvider.get());
        d->textureProvider.reset(new OutputTextureProvider(this));
    } else {
        Q_ASSERT(d->textureProvider.get());
        d->textureProvider.reset();
    }

    Q_EMIT cacheBufferChanged();
}

void WOutputViewport::setOutputScale(float scale)
{
    W_D(WOutputViewport);
    if (auto window = d->outputWindow())
        window->setOutputScale(this, scale);
}

void WOutputViewport::rotateOutput(WOutput::Transform t)
{
    W_D(WOutputViewport);
    if (auto window = d->outputWindow())
        window->rotateOutput(this, t);
}

void WOutputViewport::componentComplete()
{
    W_D(WOutputViewport);

    if (d->output)
        d->initForOutput();

    QQuickItem::componentComplete();
}

void WOutputViewport::releaseResources()
{
    W_D(WOutputViewport);

    if (d->textureProvider) {
        class WOutputViewportCleanupJob : public QRunnable
        {
        public:
            WOutputViewportCleanupJob(QObject *object) : m_object(object) { }
            void run() override {
                delete m_object;
            }
            QObject *m_object;
        };

        // Delay clean the textures on the next render after.
        d->window->scheduleRenderJob(new WOutputViewportCleanupJob(d->textureProvider.release()),
                                     QQuickWindow::AfterRenderingStage);
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputviewport.cpp"
