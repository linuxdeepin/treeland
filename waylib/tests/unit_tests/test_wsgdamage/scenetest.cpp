// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgbatchrenderer_p.h"
#include "wsgcontext_p.h"
#include "wsgdamagedebug_p.h"

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTest>
#include <QVariant>
#include <private/qquickwindow_p.h>
#include <rhi/qrhi.h>

#include <memory>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

constexpr int kSceneWidth = 800;
constexpr int kSceneHeight = 480;
constexpr int kBoundSlack = 16;

const char *kSceneQml = R"QML(
import QtQuick

Item {
    id: root
    width: 800
    height: 480

    property alias wallpaper: wallpaper
    property alias sentinel: sentinel
    property alias target: target
    property alias field: field
    property alias cursor: cursor
    property alias btnA: btnA
    property alias btnB: btnB
    property var spawned: null

    Rectangle {
        id: wallpaper
        anchors.fill: parent
        color: "#202020"
    }

    Rectangle {
        id: sentinel
        objectName: "sentinel"
        x: 700; y: 20; width: 80; height: 40
        color: "#3366aa"
    }

    Rectangle {
        id: target
        objectName: "target"
        x: 80; y: 80; width: 48; height: 48
        color: "#cc3333"
        transformOrigin: Item.Center
    }

    Rectangle {
        id: field
        objectName: "field"
        x: 200; y: 200; width: 220; height: 30
        color: "#404040"
        Rectangle {
            id: cursor
            objectName: "cursor"
            x: 8; y: 6; width: 1; height: 18
            color: "white"
        }
    }

    Rectangle {
        id: btnA
        objectName: "btnA"
        x: 640; y: 420; width: 36; height: 36
        color: "#555555"
        radius: 8
    }
    Rectangle {
        id: btnB
        objectName: "btnB"
        x: 684; y: 420; width: 36; height: 36
        color: "#555555"
        radius: 8
    }

    function spawnItem() {
        spawned = Qt.createQmlObject(
            'import QtQuick; Rectangle { objectName: "spawned"; x: 400; y: 60; width: 40; height: 40; color: "#33cc66" }',
            root)
        return spawned
    }

    function despawnItem() {
        if (spawned) {
            spawned.destroy()
            spawned = null
        }
    }

    function translateTarget(dx, dy) {
        var t = Qt.createQmlObject(
            'import QtQuick; Translate { x: ' + dx + '; y: ' + dy + ' }',
            target)
        target.transform = [t]
    }

    function resetScene() {
        target.x = 80
        target.y = 80
        target.rotation = 0
        target.scale = 1
        if (target.transform.length)
            target.transform = []
        cursor.opacity = 1
        despawnItem()
    }
}
)QML";

struct Snapshot {
    bool valid = false;
    bool full = false;
    QRegion region;

    QString dump() const
    {
        if (!valid)
            return QStringLiteral("invalid-renderer");
        return WSGDamageDebug::describe(region, full);
    }

    int area() const
    {
        int total = 0;
        for (const QRect &rect : region)
            total += rect.width() * rect.height();
        return total;
    }
};

QRect sceneRect(QQuickItem *item)
{
    if (!item)
        return {};
    return item->mapRectToScene(item->boundingRect()).toAlignedRect();
}

QRect inflatedUnion(const QRect &a, const QRect &b, int slack)
{
    return a.united(b).adjusted(-slack, -slack, slack, slack);
}

} // namespace

class WSGDamageSceneTest : public QObject
{
    Q_OBJECT

private:
    QQuickItem *named(const char *name) const
    {
        return m_root ? m_root->findChild<QQuickItem *>(QLatin1String(name)) : nullptr;
    }

    WSGBatchRenderer::Renderer *renderer() const
    {
        if (!m_window)
            return nullptr;
        return dynamic_cast<WSGBatchRenderer::Renderer *>(
            QQuickWindowPrivate::get(m_window.get())->renderer);
    }

    Snapshot take()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        m_rc->polishItems();
        m_rc->beginFrame();
        m_rc->sync();

        Snapshot snap;
        auto *batch = renderer();
        if (batch) {
            batch->commitPendingDamage();
            snap.valid = true;
            snap.full = batch->flushRegionIsFull();
            snap.region = batch->flushRegion();
        }
        m_rc->endFrame();
        return snap;
    }

    void expectValid(const Snapshot &snap, const char *what)
    {
        QVERIFY2(snap.valid, qPrintable(QStringLiteral("%1: %2").arg(QLatin1String(what), snap.dump())));
    }

    void expectNotFull(const Snapshot &snap, const char *what)
    {
        expectValid(snap, what);
        QVERIFY2(!snap.full,
                 qPrintable(QStringLiteral("%1: expected partial damage, got %2")
                                .arg(QLatin1String(what), snap.dump())));
    }

    void expectCovers(const Snapshot &snap, const QRect &rect, const char *what)
    {
        expectNotFull(snap, what);
        if (rect.isEmpty())
            return;
        QRect probe = rect.adjusted(1, 1, -1, -1);
        if (probe.isEmpty())
            probe = rect;
        QVERIFY2(snap.region.intersects(probe) || snap.region.contains(rect),
                 qPrintable(QStringLiteral("%1: damage %2 missed %3,%4 %5x%6")
                                .arg(QLatin1String(what), snap.dump())
                                .arg(rect.x())
                                .arg(rect.y())
                                .arg(rect.width())
                                .arg(rect.height())));
    }

    void expectAvoids(const Snapshot &snap, const QRect &rect, const char *what)
    {
        expectNotFull(snap, what);
        QVERIFY2(!snap.region.intersects(rect),
                 qPrintable(QStringLiteral("%1: damage %2 leaked into %3,%4 %5x%6")
                                .arg(QLatin1String(what), snap.dump())
                                .arg(rect.x())
                                .arg(rect.y())
                                .arg(rect.width())
                                .arg(rect.height())));
    }

    void expectBoundedBy(const Snapshot &snap, const QRect &box, const char *what)
    {
        expectNotFull(snap, what);
        const QRect allowed = box.adjusted(-kBoundSlack, -kBoundSlack, kBoundSlack, kBoundSlack);
        const QRect bound = snap.region.boundingRect();
        QVERIFY2(allowed.contains(bound),
                 qPrintable(QStringLiteral("%1: bounding %2,%3 %4x%5 outside %6,%7 %8x%9 (damage %10)")
                                .arg(QLatin1String(what))
                                .arg(bound.x())
                                .arg(bound.y())
                                .arg(bound.width())
                                .arg(bound.height())
                                .arg(allowed.x())
                                .arg(allowed.y())
                                .arg(allowed.width())
                                .arg(allowed.height())
                                .arg(snap.dump())));
    }

    std::unique_ptr<QQuickRenderControl> m_rc;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QRhiTexture> m_tex;
    std::unique_ptr<QRhiRenderBuffer> m_ds;
    std::unique_ptr<QRhiTextureRenderTarget> m_rt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_rp;
    QQuickItem *m_root = nullptr;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void moveItem_unionsOldAndNew();
    void addItem_damagesOnlyNewBounds();
    void removeItem_damagesOnlyOldBounds();
    void rotateItem_unionsOldAndNew();
    void scaleItem_unionsOldAndNew();
    void transformItem_unionsOldAndNew();
    void caretBlink_doesNotCoverButtons();
    void idle_isEmptyOrTiny();
};

void WSGDamageSceneTest::initTestCase()
{
    WSGContext::ensureInstalled();

    m_rc = std::make_unique<QQuickRenderControl>();
    m_window = std::make_unique<QQuickWindow>(m_rc.get());
    m_window->setColor(QColor(32, 32, 32));
    m_window->resize(kSceneWidth, kSceneHeight);
    m_window->contentItem()->setSize(QSizeF(kSceneWidth, kSceneHeight));

    if (!m_rc->initialize()) {
        qWarning("QQuickRenderControl::initialize() failed (api=%d)",
                 int(m_window->rendererInterface()
                         ? m_window->rendererInterface()->graphicsApi()
                         : QSGRendererInterface::Unknown));
        QSKIP("QQuickRenderControl::initialize() failed");
    }

    QRhi *rhi = m_rc->rhi();
    if (!rhi)
        QSKIP("QQuickRenderControl has no QRhi");

    const QSize pixelSize(kSceneWidth, kSceneHeight);
    m_tex.reset(rhi->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
    if (!m_tex || !m_tex->create())
        QSKIP("Could not create offscreen color texture");
    m_ds.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, 1));
    if (!m_ds || !m_ds->create())
        QSKIP("Could not create offscreen depth buffer");
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(m_tex.get()));
    rtDesc.setDepthStencilBuffer(m_ds.get());
    m_rt.reset(rhi->newTextureRenderTarget(rtDesc));
    if (!m_rt)
        QSKIP("Could not create offscreen render target");
    m_rp.reset(m_rt->newCompatibleRenderPassDescriptor());
    m_rt->setRenderPassDescriptor(m_rp.get());
    if (!m_rt->create())
        QSKIP("Could not create offscreen render pass");
    m_window->setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(m_rt.get()));

    m_engine = std::make_unique<QQmlEngine>();
    QQmlComponent component(m_engine.get());
    component.setData(QByteArray(kSceneQml), QUrl(QStringLiteral("qrc:/wsgdamage/scene.qml")));
    QObject *obj = component.create();
    if (!obj) {
        const QString errors = component.errorString();
        QFAIL(qPrintable(QStringLiteral("failed to create scene QML: %1").arg(errors)));
    }
    m_root = qobject_cast<QQuickItem *>(obj);
    QVERIFY(m_root);
    m_root->setParentItem(m_window->contentItem());
    m_root->setSize(QSizeF(kSceneWidth, kSceneHeight));

    for (int i = 0; i < 3; ++i)
        take();

    if (!renderer())
        QSKIP("WSGBatchRenderer was not installed for this scene graph");
}

void WSGDamageSceneTest::cleanupTestCase()
{
    if (m_root) {
        m_root->setParentItem(nullptr);
        delete m_root;
        m_root = nullptr;
    }
    if (m_window)
        m_window->setRenderTarget(QQuickRenderTarget());
    m_rt.reset();
    m_rp.reset();
    m_ds.reset();
    m_tex.reset();
    m_engine.reset();
    m_window.reset();
    m_rc.reset();
}

void WSGDamageSceneTest::init()
{
    QVERIFY(m_root);
    QVERIFY(QMetaObject::invokeMethod(m_root, "resetScene", Qt::DirectConnection));
    take();
}

void WSGDamageSceneTest::addItem_damagesOnlyNewBounds()
{
    QVERIFY(QMetaObject::invokeMethod(m_root, "spawnItem", Qt::DirectConnection));
    auto *spawned = named("spawned");
    QVERIFY(spawned);
    const QRect now = sceneRect(spawned);
    const Snapshot snap = take();

    expectCovers(snap, now, "add");
    expectAvoids(snap, sceneRect(named("sentinel")), "add sentinel");
    expectAvoids(snap, sceneRect(named("btnA")), "add button");
    expectBoundedBy(snap, now, "add");
}

void WSGDamageSceneTest::removeItem_damagesOnlyOldBounds()
{
    QVERIFY(QMetaObject::invokeMethod(m_root, "spawnItem", Qt::DirectConnection));
    auto *spawned = named("spawned");
    QVERIFY(spawned);
    const QRect oldRect = sceneRect(spawned);
    take();

    QVERIFY(QMetaObject::invokeMethod(m_root, "despawnItem", Qt::DirectConnection));
    const Snapshot snap = take();

    expectCovers(snap, oldRect, "remove");
    expectAvoids(snap, sceneRect(named("sentinel")), "remove sentinel");
    expectAvoids(snap, sceneRect(named("btnA")), "remove button");
    expectBoundedBy(snap, oldRect, "remove");
}

void WSGDamageSceneTest::moveItem_unionsOldAndNew()
{
    auto *target = named("target");
    QVERIFY(target);
    const QRect oldRect = sceneRect(target);
    target->setX(target->x() + 80);
    target->setY(target->y() + 40);
    const QRect now = sceneRect(target);
    const Snapshot snap = take();

    expectCovers(snap, oldRect, "move old");
    expectCovers(snap, now, "move new");
    expectAvoids(snap, sceneRect(named("sentinel")), "move sentinel");
    expectBoundedBy(snap, inflatedUnion(oldRect, now, 0), "move");
}

void WSGDamageSceneTest::rotateItem_unionsOldAndNew()
{
    auto *target = named("target");
    QVERIFY(target);
    const QRect oldRect = sceneRect(target);
    target->setRotation(45);
    const QRect now = sceneRect(target);
    const Snapshot snap = take();

    expectCovers(snap, oldRect, "rotate old");
    expectCovers(snap, now, "rotate new");
    expectAvoids(snap, sceneRect(named("sentinel")), "rotate sentinel");
    expectAvoids(snap, sceneRect(named("btnA")), "rotate button");
    expectBoundedBy(snap, inflatedUnion(oldRect, now, 0), "rotate");
}

void WSGDamageSceneTest::scaleItem_unionsOldAndNew()
{
    auto *target = named("target");
    QVERIFY(target);
    const QRect oldRect = sceneRect(target);
    target->setScale(2);
    const QRect now = sceneRect(target);
    const Snapshot snap = take();

    expectCovers(snap, oldRect, "scale old");
    expectCovers(snap, now, "scale new");
    expectAvoids(snap, sceneRect(named("sentinel")), "scale sentinel");
    expectAvoids(snap, sceneRect(named("btnA")), "scale button");
    expectBoundedBy(snap, inflatedUnion(oldRect, now, 0), "scale");
}

void WSGDamageSceneTest::transformItem_unionsOldAndNew()
{
    auto *target = named("target");
    QVERIFY(target);
    const QRect oldRect = sceneRect(target);
    QVERIFY(QMetaObject::invokeMethod(m_root, "translateTarget", Qt::DirectConnection,
                                      Q_ARG(QVariant, 40), Q_ARG(QVariant, 30)));
    const QRect now = sceneRect(target);
    const Snapshot snap = take();

    expectCovers(snap, oldRect, "transform old");
    expectCovers(snap, now, "transform new");
    expectAvoids(snap, sceneRect(named("sentinel")), "transform sentinel");
    expectAvoids(snap, sceneRect(named("btnA")), "transform button");
    expectBoundedBy(snap, inflatedUnion(oldRect, now, 0), "transform");
}

void WSGDamageSceneTest::caretBlink_doesNotCoverButtons()
{
    auto *cursor = named("cursor");
    QVERIFY(cursor);
    cursor->setOpacity(cursor->opacity() > 0.5 ? 0.0 : 1.0);
    const Snapshot snap = take();

    expectNotFull(snap, "caret");
    expectAvoids(snap, sceneRect(named("btnA")), "caret button A");
    expectAvoids(snap, sceneRect(named("btnB")), "caret button B");
    expectAvoids(snap, sceneRect(named("sentinel")), "caret sentinel");
    QVERIFY2(snap.area() <= 220 * 30 * 2,
             qPrintable(QStringLiteral("caret: area %1 too large (%2)")
                            .arg(snap.area())
                            .arg(snap.dump())));
}

void WSGDamageSceneTest::idle_isEmptyOrTiny()
{
    const Snapshot snap = take();
    expectValid(snap, "idle");
    QVERIFY2(!snap.full,
             qPrintable(QStringLiteral("idle: full damage %1").arg(snap.dump())));
    QVERIFY2(snap.area() <= 64,
             qPrintable(QStringLiteral("idle: unexpected damage %1 area=%2")
                            .arg(snap.dump())
                            .arg(snap.area())));
}

int runDamageSceneTests(int argc, char **argv)
{
    WSGDamageSceneTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "scenetest.moc"
