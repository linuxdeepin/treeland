// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "player.h"

#include "capture.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <libdrm/drm_fourcc.h>
#include <rhi/qrhi.h>

#include <QSGImageNode>
#include <QSGTexture>

Player::Player()
{
    setFlag(QQuickItem::ItemHasContents);
}

TreelandCaptureContext *Player::captureContext() const
{
    return m_captureContext;
}

void Player::setCaptureContext(TreelandCaptureContext *context)
{
    if (context == m_captureContext)
        return;
    if (m_captureContext) {
        m_captureContext->disconnect(this);
    }
    m_captureContext = context;
    if (m_captureContext) {
        connect(m_captureContext.data(),
                &TreelandCaptureContext::destroyed,
                this,
                std::bind(&Player::setCaptureContext, this, nullptr));
        if (m_captureContext->session()) {
            connect(m_captureContext->session(),
                    &TreelandCaptureSession::ready,
                    this,
                    &Player::update);
        } else {
            connect(m_captureContext, &TreelandCaptureContext::sessionChanged, this, [this] {
                if (m_captureContext->session())
                    connect(m_captureContext->session(),
                            &TreelandCaptureSession::ready,
                            this,
                            &Player::update);
            });
        }
        connect(m_captureContext.data(),
                &TreelandCaptureContext::sourceReady,
                this,
                &Player::updateGeometry);
    }
    Q_EMIT captureContextChanged();
}

QSGNode *Player::updatePaintNode(QSGNode *old, UpdatePaintNodeData *data)
{
    ensureDebugLogger();
    if (!m_captureContext || !m_captureContext->session()
        || !m_captureContext->session()->started())
        return nullptr;
    updateTexture();
    auto node = static_cast<QSGImageNode *>(old);
    if (Q_UNLIKELY(!node)) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
        node->setTexture(&m_texture);
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }
    QRectF sourceRect;
    if (m_captureContext->sourceType()
        == QtWayland::treeland_capture_context_v1::source_type_window) {
        sourceRect = QRectF({ 0, 0 }, m_texture.textureSize());
    } else {
        sourceRect = m_captureContext->captureRegion();
    }
    node->setSourceRect(sourceRect);
    node->setRect(boundingRect());
    node->setFiltering(QSGTexture::Linear);
    node->setMipmapFiltering(QSGTexture::None);
    return node;
}

#define CASE_STR(value) \
    case value:         \
        return #value;

static const char *eglGetErrorString(EGLint error)
{
    switch (error) {
        CASE_STR(EGL_SUCCESS)
        CASE_STR(EGL_NOT_INITIALIZED)
        CASE_STR(EGL_BAD_ACCESS)
        CASE_STR(EGL_BAD_ALLOC)
        CASE_STR(EGL_BAD_ATTRIBUTE)
        CASE_STR(EGL_BAD_CONTEXT)
        CASE_STR(EGL_BAD_CONFIG)
        CASE_STR(EGL_BAD_CURRENT_SURFACE)
        CASE_STR(EGL_BAD_DISPLAY)
        CASE_STR(EGL_BAD_SURFACE)
        CASE_STR(EGL_BAD_MATCH)
        CASE_STR(EGL_BAD_PARAMETER)
        CASE_STR(EGL_BAD_NATIVE_PIXMAP)
        CASE_STR(EGL_BAD_NATIVE_WINDOW)
        CASE_STR(EGL_CONTEXT_LOST)
    default:
        return "Unknown";
    }
}

static const char *drmFormatString(int fourcc)
{
    switch (fourcc) {
        CASE_STR(DRM_FORMAT_R8)
        CASE_STR(DRM_FORMAT_RG88)
        CASE_STR(DRM_FORMAT_GR88)
        CASE_STR(DRM_FORMAT_R16)
        CASE_STR(DRM_FORMAT_GR1616)
        CASE_STR(DRM_FORMAT_RGB332)
        CASE_STR(DRM_FORMAT_BGR233)
        CASE_STR(DRM_FORMAT_XRGB4444)
        CASE_STR(DRM_FORMAT_XBGR4444)
        CASE_STR(DRM_FORMAT_RGBX4444)
        CASE_STR(DRM_FORMAT_BGRX4444)
        CASE_STR(DRM_FORMAT_ARGB4444)
        CASE_STR(DRM_FORMAT_ABGR4444)
        CASE_STR(DRM_FORMAT_RGBA4444)
        CASE_STR(DRM_FORMAT_BGRA4444)
        CASE_STR(DRM_FORMAT_XRGB1555)
        CASE_STR(DRM_FORMAT_XBGR1555)
        CASE_STR(DRM_FORMAT_RGBX5551)
        CASE_STR(DRM_FORMAT_BGRX5551)
        CASE_STR(DRM_FORMAT_ARGB1555)
        CASE_STR(DRM_FORMAT_ABGR1555)
        CASE_STR(DRM_FORMAT_RGBA5551)
        CASE_STR(DRM_FORMAT_BGRA5551)
        CASE_STR(DRM_FORMAT_RGB565)
        CASE_STR(DRM_FORMAT_BGR565)
        CASE_STR(DRM_FORMAT_RGB888)
        CASE_STR(DRM_FORMAT_BGR888)
        CASE_STR(DRM_FORMAT_XRGB8888)
        CASE_STR(DRM_FORMAT_XBGR8888)
        CASE_STR(DRM_FORMAT_RGBX8888)
        CASE_STR(DRM_FORMAT_BGRX8888)
        CASE_STR(DRM_FORMAT_ARGB8888)
        CASE_STR(DRM_FORMAT_ABGR8888)
        CASE_STR(DRM_FORMAT_RGBA8888)
        CASE_STR(DRM_FORMAT_BGRA8888)
        CASE_STR(DRM_FORMAT_XRGB2101010)
        CASE_STR(DRM_FORMAT_XBGR2101010)
        CASE_STR(DRM_FORMAT_RGBX1010102)
        CASE_STR(DRM_FORMAT_BGRX1010102)
        CASE_STR(DRM_FORMAT_ARGB2101010)
        CASE_STR(DRM_FORMAT_ABGR2101010)
        CASE_STR(DRM_FORMAT_RGBA1010102)
        CASE_STR(DRM_FORMAT_BGRA1010102)
        CASE_STR(DRM_FORMAT_ABGR16161616)
        CASE_STR(DRM_FORMAT_XBGR16161616)
        CASE_STR(DRM_FORMAT_XBGR16161616F)
        CASE_STR(DRM_FORMAT_ABGR16161616F)
        CASE_STR(DRM_FORMAT_YUYV)
        CASE_STR(DRM_FORMAT_YVYU)
        CASE_STR(DRM_FORMAT_UYVY)
        CASE_STR(DRM_FORMAT_VYUY)
        CASE_STR(DRM_FORMAT_AYUV)
        CASE_STR(DRM_FORMAT_XYUV8888)
        CASE_STR(DRM_FORMAT_Y210)
        CASE_STR(DRM_FORMAT_Y212)
        CASE_STR(DRM_FORMAT_Y216)
        CASE_STR(DRM_FORMAT_Y410)
        CASE_STR(DRM_FORMAT_Y412)
        CASE_STR(DRM_FORMAT_Y416)
        CASE_STR(DRM_FORMAT_NV12)
        CASE_STR(DRM_FORMAT_NV21)
        CASE_STR(DRM_FORMAT_NV16)
        CASE_STR(DRM_FORMAT_NV61)
        CASE_STR(DRM_FORMAT_P010)
        CASE_STR(DRM_FORMAT_P012)
        CASE_STR(DRM_FORMAT_P016)
        CASE_STR(DRM_FORMAT_P030)
        CASE_STR(DRM_FORMAT_YUV410)
        CASE_STR(DRM_FORMAT_YVU410)
        CASE_STR(DRM_FORMAT_YUV411)
        CASE_STR(DRM_FORMAT_YVU411)
        CASE_STR(DRM_FORMAT_YUV420)
        CASE_STR(DRM_FORMAT_YVU420)
        CASE_STR(DRM_FORMAT_YUV422)
        CASE_STR(DRM_FORMAT_YVU422)
        CASE_STR(DRM_FORMAT_YUV444)
        CASE_STR(DRM_FORMAT_YVU444)
    default:
        return "Unknown";
    }
}

#undef CASE_STR

void Player::updateTexture()
{
    auto glContext = QOpenGLContext::currentContext();
    Q_ASSERT(glContext);
    auto eglContext = glContext->nativeInterface<QNativeInterface::QEGLContext>();
    Q_ASSERT(eglContext);
    EGLImage eglImage;
    EGLAttrib attribs[47];
    int atti = 0;
    attribs[atti++] = EGL_WIDTH;
    attribs[atti++] = m_captureContext->session()->bufferWidth();
    attribs[atti++] = EGL_HEIGHT;
    attribs[atti++] = m_captureContext->session()->bufferHeight();
    attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[atti++] = m_captureContext->session()->bufferFormat();

    auto objects = m_captureContext->session()->objects();
    auto nPlanes = objects.size();
    if (nPlanes > 0) {
        attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
        attribs[atti++] = objects[0].fd;
        attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
        attribs[atti++] = objects[0].offset;
        attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
        attribs[atti++] = objects[0].stride;
        if (m_captureContext->session()->modifierUnion().modifier) {
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
        }
    }

    if (nPlanes > 1) {
        attribs[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
        attribs[atti++] = objects[1].fd;
        attribs[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
        attribs[atti++] = objects[1].offset;
        attribs[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
        attribs[atti++] = objects[1].stride;
        if (m_captureContext->session()->modifierUnion().modifier) {
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
        }
    }

    if (nPlanes > 2) {
        attribs[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
        attribs[atti++] = objects[2].fd;
        attribs[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
        attribs[atti++] = objects[2].offset;
        attribs[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
        attribs[atti++] = objects[2].stride;
        if (m_captureContext->session()->modifierUnion().modifier) {
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
        }
    }

    if (nPlanes > 3) {
        attribs[atti++] = EGL_DMA_BUF_PLANE3_FD_EXT;
        attribs[atti++] = objects[3].fd;
        attribs[atti++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
        attribs[atti++] = objects[3].offset;
        attribs[atti++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
        attribs[atti++] = objects[3].stride;
        if (m_captureContext->session()->modifierUnion().modifier) {
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
        }
    }

    attribs[atti++] = EGL_NONE;
    eglImage =
        eglCreateImage(eglContext->display(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, attribs);
    if (eglImage == EGL_NO_IMAGE) {
        qWarning() << eglGetErrorString(eglGetError());
        for (const auto &object : std::as_const(objects)) {
            if (object.fd > 0)
                close(object.fd);
        }
        return;
    }
    Q_ASSERT_X(window(), __func__, "Window should be ready for rhi.");
    if (m_rhiTexture) {
        m_texture.setTexture(nullptr);
        delete m_rhiTexture;
    }
    m_rhiTexture =
        window()->rhi()->newTexture(QRhiTexture::RGBA8,
                                    QSize{ int(m_captureContext->session()->bufferWidth()),
                                           int(m_captureContext->session()->bufferHeight()) },
                                    1,
                                    QRhiTexture::TextureArray);
    glBindTexture(GL_TEXTURE_2D, m_rhiTexture->nativeTexture().object);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
    m_texture.setTexture(m_rhiTexture);
    m_texture.setTextureSize(QSize{ int(m_captureContext->session()->bufferWidth()),
                                    int(m_captureContext->session()->bufferHeight()) });
    m_texture.setOwnsTexture(false);
    glBindTexture(GL_TEXTURE_2D, 0);
    for (const auto &object : std::as_const(objects)) {
        if (object.fd > 0)
            close(object.fd);
    }
}

void Player::updateGeometry()
{
    Q_ASSERT(m_captureContext);
    setWidth(m_captureContext->captureRegion().width());
    setHeight(m_captureContext->captureRegion().height());
}

static void EGLAPIENTRY debugCallback(EGLenum error,
                                      [[maybe_unused]] const char *command,
                                      [[maybe_unused]] EGLint messageType,
                                      [[maybe_unused]] EGLLabelKHR threadLabel,
                                      [[maybe_unused]] EGLLabelKHR objectLabel,
                                      const char *message)
{
    qInfo() << "EGL Debug:" << message << "(Error Code:" << error << ")";
}

void Player::ensureDebugLogger()
{
    if (m_loggerInitialized)
        return;
    if (QOpenGLContext::currentContext()->hasExtension(QByteArrayLiteral("GL_KHR_debug"))) {
        const EGLAttrib debugAttribs[] = {
            EGL_DEBUG_MSG_CRITICAL_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_ERROR_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_WARN_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_INFO_KHR,
            EGL_TRUE,
            EGL_NONE,
        };
        PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR =
            (PFNEGLDEBUGMESSAGECONTROLKHRPROC)eglGetProcAddress("eglDebugMessageControlKHR");
        if (eglDebugMessageControlKHR) {
            eglDebugMessageControlKHR(debugCallback, debugAttribs);
        } else {
            qCritical() << "Failed to get eglDebugMessageControlKHR function.";
        }
    } else {
        qCritical() << "GL_KHR_debug is not supported.";
    }
    m_loggerInitialized = true;
}
