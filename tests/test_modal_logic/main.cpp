// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surface/surfacewrapper.h"

#include <QObject>
#include <QTest>

// Pure logic-layer unit tests for the KWin-style modal policy introduced by
// xdg-dialog-v1. These exercise the decision helpers (findModal recursion,
// minimize linkage, activation redirection) on bare SurfaceWrapper instances
// without a running Wayland compositor, so they can run in CI with the
// offscreen QPA platform.
class ModalLogicTest : public QObject
{
    Q_OBJECT

    QList<SurfaceWrapper *> m_wrappers;

    // Create a bare wrapper (no shell surface / QML setup) for logic tests.
    SurfaceWrapper *makeWrapper(int workspaceId = -1, bool minimized = false)
    {
        auto *w = new SurfaceWrapper(SurfaceWrapper::TestTag{});
        w->m_workspaceId = workspaceId;
        if (minimized)
            w->m_surfaceState.setValueBypassingBindings(SurfaceWrapper::State::Minimized);
        m_wrappers.append(w);
        return w;
    }

    // Create a wrapper and attach it to a parent (sets up m_subSurfaces/m_parentSurface).
    SurfaceWrapper *makeChild(SurfaceWrapper *parent, int workspaceId = -1, bool minimized = false)
    {
        auto *w = makeWrapper(workspaceId, minimized);
        if (parent)
            parent->addSubSurface(w);
        return w;
    }

    void setMinimized(SurfaceWrapper *w, bool minimized)
    {
        w->m_surfaceState.setValueBypassingBindings(
            minimized ? SurfaceWrapper::State::Minimized : SurfaceWrapper::State::Normal);
    }

private Q_SLOTS:

    void cleanupTestCase()
    {
        for (auto *w : m_wrappers)
            delete w;
        m_wrappers.clear();
    }

    void findModalReturnsNullWhenNoModal()
    {
        auto *parent = makeWrapper();
        makeChild(parent); // not modal
        QVERIFY(parent->findModal() == nullptr);
    }

    void findModalReturnsDirectModalChild()
    {
        auto *parent = makeWrapper();
        auto *child = makeChild(parent);
        child->setModal(true);
        QCOMPARE(parent->findModal(), child);
    }

    void findModalReturnsDeepestModal()
    {
        auto *parent = makeWrapper();
        auto *mid = makeChild(parent);
        auto *leaf = makeChild(mid);
        leaf->setModal(true);
        QCOMPARE(parent->findModal(), leaf);
    }

    void findModalSkipsAboutToRemove()
    {
        auto *parent = makeWrapper();
        auto *child = makeChild(parent);
        child->setModal(true);
        child->m_wrapperAboutToRemove = true;
        QVERIFY(parent->findModal() == nullptr);
    }

    void linkedChildrenSkipsModalWhenMinimizing()
    {
        auto *parent = makeWrapper();
        auto *normal = makeChild(parent);
        auto *modal = makeChild(parent);
        modal->setModal(true);

        const auto toToggle = parent->linkedChildrenForMinimize(true);
        QVERIFY(toToggle.contains(normal));
        QVERIFY(!toToggle.contains(modal));
    }

    void linkedChildrenIncludesModalWhenRestoring()
    {
        auto *parent = makeWrapper();
        auto *normal = makeChild(parent, -1, true); // minimized
        auto *modal = makeChild(parent, -1, true);  // minimized
        modal->setModal(true);

        const auto toToggle = parent->linkedChildrenForMinimize(false);
        QVERIFY(toToggle.contains(normal));
        QVERIFY(toToggle.contains(modal));
    }

    void linkedChildrenOmitsAlreadyMatching()
    {
        auto *parent = makeWrapper();
        auto *alreadyMin = makeChild(parent, -1, true); // already minimized
        auto *visible = makeChild(parent, -1, false);

        // parent minimizing: the visible child toggles, the already-minimized one does not
        const auto minimizing = parent->linkedChildrenForMinimize(true);
        QVERIFY(minimizing.contains(visible));
        QVERIFY(!minimizing.contains(alreadyMin));
    }

    void linkedParentForMinimize()
    {
        auto *parent = makeWrapper();
        auto *modal = makeChild(parent);
        modal->setModal(true);

        // modal minimizing pulls parent along
        QCOMPARE(modal->linkedParentForMinimize(true), parent);

        // parent already minimized -> nothing to do
        setMinimized(parent, true);
        QVERIFY(modal->linkedParentForMinimize(true) == nullptr);

        // modal restoring restores parent
        QCOMPARE(modal->linkedParentForMinimize(false), parent);

        // parent no longer minimized -> nothing to do
        setMinimized(parent, false);
        QVERIFY(modal->linkedParentForMinimize(false) == nullptr);
    }

    void linkedParentNullForNonModal()
    {
        auto *parent = makeWrapper();
        auto *child = makeChild(parent);
        QVERIFY(child->linkedParentForMinimize(true) == nullptr);
    }

    void redirectNoneWhenNoModal()
    {
        auto *parent = makeWrapper(1);
        auto r = parent->computeModalRedirect();
        QVERIFY(r.modal == nullptr);
        QVERIFY(!r.refuseActivation);
    }

    void redirectSameWorkspace()
    {
        auto *parent = makeWrapper(1);
        auto *modal = makeChild(parent, 1);
        modal->setModal(true);

        auto r = parent->computeModalRedirect();
        QCOMPARE(r.modal, modal);
        QVERIFY(!r.needsWorkspaceMove);
        QVERIFY(!r.refuseActivation);
    }

    void redirectDifferentWorkspaceNeedsMove()
    {
        auto *parent = makeWrapper(1);
        auto *modal = makeChild(parent, 2);
        modal->setModal(true);

        auto r = parent->computeModalRedirect();
        QCOMPARE(r.modal, modal);
        QVERIFY(r.needsWorkspaceMove);
        QCOMPARE(r.targetWorkspaceId, 1);
        QVERIFY(!r.refuseActivation);
    }

    void redirectNoMoveWhenWorkspaceUnknown()
    {
        // wrapper->workspaceId() == -1 -> never move the modal
        auto *parent = makeWrapper(-1);
        auto *modal = makeChild(parent, 2);
        modal->setModal(true);

        auto r = parent->computeModalRedirect();
        QCOMPARE(r.modal, modal);
        QVERIFY(!r.needsWorkspaceMove);
    }

    void redirectRefusesWhenModalMinimized()
    {
        auto *parent = makeWrapper(1);
        auto *modal = makeChild(parent, 1);
        modal->setModal(true);
        setMinimized(modal, true);

        auto r = parent->computeModalRedirect();
        QCOMPARE(r.modal, modal);
        QVERIFY(r.refuseActivation);
    }
};

QTEST_MAIN(ModalLogicTest)
#include "main.moc"
