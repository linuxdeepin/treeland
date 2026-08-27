// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagetracker.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QTest>

#include <functional>
#include <vector>
WAYLIB_SERVER_USE_NAMESPACE

class WSGDamageGraphBench : public QObject
{
    Q_OBJECT

private slots:
    void benchmarkSuite();
};

struct BenchmarkResult
{
    QString category;
    QString scenario;
    int nodeCount;
    int viewportCount;
    double avgTimeUs;
    double maxFps;
    double budget60Hz;
    double budget144Hz;
    double budget240Hz;
};

static std::vector<BenchmarkResult> s_results;

static WSGDamageNode *buildRealisticTree(
    int totalNodes,
    std::vector<WSGDamageGeometryNode *> *outGeos = nullptr,
    std::vector<WSGDamageTransformNode *> *outTransforms = nullptr,
    std::vector<WSGDamageGeometryNode *> *outBackdrops = nullptr)
{
    auto *root = new WSGDamageNode(WSGDamageNode::Type::Basic);
    root->setName(QStringLiteral("root"));

    int created = 0;
    int containerId = 0;

    while (created < totalNodes) {
        auto *layer = new WSGDamageTransformNode();
        layer->setName(QStringLiteral("container_%1").arg(++containerId));
        layer->setTranslation((created % 10) * 80.0, (created / 10) * 60.0);
        root->appendChild(layer);
        if (outTransforms)
            outTransforms->push_back(layer);
        created++;

        const int childrenInLayer = std::min(15, totalNodes - created);
        for (int i = 0; i < childrenInLayer; ++i) {
            if (i == 3 && outBackdrops && created + 1 < totalNodes) {
                auto *bg = new WSGDamageGeometryNode();
                bg->setName(QStringLiteral("backdrop_%1").arg(created));
                bg->setBoundingRect(QRectF(10 * i, 10 * i, 200, 150));
                bg->setNeedsBackdrop(true);
                layer->appendChild(bg);
                outBackdrops->push_back(bg);
                created++;
                continue;
            }

            auto *geo = new WSGDamageGeometryNode();
            geo->setName(QStringLiteral("item_%1").arg(created));
            geo->setBoundingRect(QRectF(15 * i, 10 * i, 120, 80));
            geo->setFullyOpaque(i % 2 == 0);
            layer->appendChild(geo);
            if (outGeos)
                outGeos->push_back(geo);
            created++;
            if (created >= totalNodes)
                break;
        }
    }

    return root;
}

static QVector<WSGViewport> createViewports(int count)
{
    QVector<WSGViewport> vps;
    vps.reserve(count);
    for (int i = 0; i < count; ++i) {
        WSGViewport vp(QRect(0, 0, 1920, 1080));
        vps.push_back(vp);
    }
    return vps;
}

static int getIterations(int nodes)
{
    if (nodes <= 50)
        return 600;
    if (nodes <= 200)
        return 300;
    if (nodes <= 1000)
        return 80;
    return 20;
}

static BenchmarkResult runBenchmark(
    const QString &category,
    const QString &scenario,
    int nodeCount,
    int viewportCount,
    int iterations,
    const std::function<void(WSGDamageTracker &,
                             WSGDamageNode *,
                             const std::vector<WSGDamageGeometryNode *> &,
                             const std::vector<WSGDamageTransformNode *> &,
                             const std::vector<WSGDamageGeometryNode *> &,
                             int)> &prepareIteration)
{
    std::vector<WSGDamageGeometryNode *> geos;
    std::vector<WSGDamageTransformNode *> transforms;
    std::vector<WSGDamageGeometryNode *> backdrops;

    std::unique_ptr<WSGDamageNode> root(
        buildRealisticTree(nodeCount, &geos, &transforms, &backdrops));
    WSGDamageTracker tracker(root.get());

    auto viewports = createViewports(viewportCount);
    auto cycle = [&] {
        tracker.prepareFrame();
        for (auto &vp : viewports)
            tracker.commit(vp);
        tracker.finishFrame();
        for (auto &vp : viewports)
            vp.finishFrame();
    };
    cycle(); // warm-up & initial commit

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < iterations; ++i) {
        prepareIteration(tracker, root.get(), geos, transforms, backdrops, i);
        cycle();
    }

    const qint64 elapsedNs = timer.nsecsElapsed();
    const double avgUs = double(elapsedNs) / (double(iterations) * 1000.0);
    const double maxFps = 1000000.0 / std::max(0.001, avgUs);

    BenchmarkResult res;
    res.category = category;
    res.scenario = scenario;
    res.nodeCount = nodeCount;
    res.viewportCount = viewportCount;
    res.avgTimeUs = avgUs;
    res.maxFps = maxFps;
    res.budget60Hz = (avgUs / 16666.67) * 100.0;
    res.budget144Hz = (avgUs / 6944.44) * 100.0;
    res.budget240Hz = (avgUs / 4166.67) * 100.0;
    return res;
}

enum class LargeSubtreeMode
{
    ExactMove,
    ExactContent,
    RendererAggregateMove,
    RendererAggregateContent
};

static BenchmarkResult runLargeSubtreeBenchmark(const QString &scenario,
                                                int nodeCount,
                                                LargeSubtreeMode mode)
{
    auto root = std::make_unique<WSGDamageNode>();
    auto *group = new WSGDamageTransformNode;
    root->appendChild(group);

    constexpr int columns = 100;
    constexpr int cellSize = 8;
    const int rows = (nodeCount + columns - 1) / columns;
    const QRect aggregateBounds(0, 0, columns * cellSize, rows * cellSize);
    const bool aggregated = mode == LargeSubtreeMode::RendererAggregateMove
        || mode == LargeSubtreeMode::RendererAggregateContent;

    std::vector<WSGDamageGeometryNode *> geos;
    WSGDamageGeometryNode *proxy = nullptr;
    if (aggregated) {
        proxy = new WSGDamageGeometryNode;
        proxy->setBoundingRect(aggregateBounds);
        group->appendChild(proxy);
    } else {
        geos.reserve(nodeCount);
        for (int i = 0; i < nodeCount; ++i) {
            auto *geo = new WSGDamageGeometryNode;
            geo->setBoundingRect(QRectF((i % columns) * cellSize, (i / columns) * cellSize, 6, 6));
            group->appendChild(geo);
            geos.push_back(geo);
        }
    }

    WSGDamageTracker tracker(root.get());
    auto viewports = createViewports(1);
    auto cycle = [&] {
        tracker.prepareFrame();
        for (auto &vp : viewports)
            tracker.commit(vp);
        tracker.finishFrame();
        for (auto &viewport : viewports)
            viewport.finishFrame();
    };
    cycle();

    constexpr int iterations = 20;
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        if (mode == LargeSubtreeMode::ExactContent) {
            geos[i % geos.size()]->markContentDirty(QRect(1, 1, 2, 2));
        } else if (mode == LargeSubtreeMode::RendererAggregateContent) {
            proxy->markContentDirty(aggregateBounds);
        } else {
            group->setTranslation((i & 1) ? 0 : 20, 0);
        }
        cycle();
    }

    const double avgUs = double(timer.nsecsElapsed()) / (double(iterations) * 1000.0);
    return {
        QStringLiteral("万节点 Renderer 子树聚合"),
        scenario,
        nodeCount,
        1,
        avgUs,
        1000000.0 / std::max(0.001, avgUs),
        (avgUs / 16666.67) * 100.0,
        (avgUs / 6944.44) * 100.0,
        (avgUs / 4166.67) * 100.0,
    };
}

void WSGDamageGraphBench::benchmarkSuite()
{
    qInfo() << "==================================================================================="
               "=======";
    qInfo() << "                 GUI Damage WSGDamageTracker Performance Benchmark Suite           "
               "                ";
    qInfo() << "==================================================================================="
               "=======";

    // 1. Idle Frame Fast-Path
    for (int nodes : { 50, 200, 1000, 5000 }) {
        for (int vps : { 1, 2, 4 }) {
            s_results.push_back(runBenchmark(QStringLiteral("空闲帧 (无脏位快速路径)"),
                                             QStringLiteral("Idle commit (0 changes)"),
                                             nodes,
                                             vps,
                                             getIterations(nodes) * 2,
                                             [](WSGDamageTracker &,
                                                WSGDamageNode *,
                                                const std::vector<WSGDamageGeometryNode *> &,
                                                const std::vector<WSGDamageTransformNode *> &,
                                                const std::vector<WSGDamageGeometryNode *> &,
                                                int) {
                                                 // no-op
                                             }));
        }
    }

    // 2. Single Leaf Content Damage (e.g., text cursor blink or small widget repaint)
    for (int nodes : { 50, 200, 1000, 5000 }) {
        for (int vps : { 1, 2, 4 }) {
            s_results.push_back(runBenchmark(QStringLiteral("局部内容损伤 (16x16 px)"),
                                             QStringLiteral("Single leaf node markContentDirty"),
                                             nodes,
                                             vps,
                                             getIterations(nodes),
                                             [](WSGDamageTracker &,
                                                WSGDamageNode *,
                                                const std::vector<WSGDamageGeometryNode *> &geos,
                                                const std::vector<WSGDamageTransformNode *> &,
                                                const std::vector<WSGDamageGeometryNode *> &,
                                                int iter) {
                                                 if (!geos.empty()) {
                                                     auto *target = geos[iter % geos.size()];
                                                     target->markContentDirty(
                                                         QRect(10, 10, 16, 16));
                                                 }
                                             }));
        }
    }

    // 3. WSGDamageNode Geometry Translation (Window drag / widget motion)
    for (int nodes : { 50, 200, 1000, 5000 }) {
        for (int vps : { 1, 2, 4 }) {
            s_results.push_back(runBenchmark(
                QStringLiteral("单节点几何移动 (平移)"),
                QStringLiteral("Single node setBoundingRect translate"),
                nodes,
                vps,
                getIterations(nodes),
                [](WSGDamageTracker &,
                   WSGDamageNode *,
                   const std::vector<WSGDamageGeometryNode *> &geos,
                   const std::vector<WSGDamageTransformNode *> &,
                   const std::vector<WSGDamageGeometryNode *> &,
                   int iter) {
                    if (!geos.empty()) {
                        auto *target = geos[iter % geos.size()];
                        target->setBoundingRect(
                            QRectF(15.0 + (iter % 50), 10.0 + (iter % 30), 120.0, 80.0));
                    }
                }));
        }
    }

    // 4. Subtree Transform Rotation
    for (int nodes : { 50, 200, 1000 }) {
        for (int vps : { 1, 2 }) {
            s_results.push_back(
                runBenchmark(QStringLiteral("子树层级旋转 (WSGDamageTransformNode)"),
                             QStringLiteral("Subtree rotation matrix change"),
                             nodes,
                             vps,
                             getIterations(nodes),
                             [](WSGDamageTracker &,
                                WSGDamageNode *,
                                const std::vector<WSGDamageGeometryNode *> &,
                                const std::vector<WSGDamageTransformNode *> &transforms,
                                const std::vector<WSGDamageGeometryNode *> &,
                                int iter) {
                                 if (!transforms.empty()) {
                                     auto *target = transforms[iter % transforms.size()];
                                     QTransform matrix;
                                     matrix.translate(200, 200);
                                     matrix.rotate(double(iter % 360), Qt::ZAxis);
                                     target->setMatrix(matrix);
                                 }
                             }));
        }
    }

    // 5. Backdrop Induced Damage Dilation
    for (int nodes : { 50, 200, 1000 }) {
        for (int vps : { 1, 2 }) {
            s_results.push_back(runBenchmark(QStringLiteral("背景采样扩散 (Backdrop 16px)"),
                                             QStringLiteral("Backdrop induced damage dilation"),
                                             nodes,
                                             vps,
                                             getIterations(nodes),
                                             [](WSGDamageTracker &,
                                                WSGDamageNode *,
                                                const std::vector<WSGDamageGeometryNode *> &geos,
                                                const std::vector<WSGDamageTransformNode *> &,
                                                const std::vector<WSGDamageGeometryNode *> &,
                                                int) {
                                                 if (!geos.empty()) {
                                                     geos[0]->markContentDirty(
                                                         QRect(10, 10, 40, 40));
                                                 }
                                             }));
        }
    }

    // 6. High-Density Scattered Damage (10% of all nodes dirty simultaneously)
    for (int nodes : { 50, 200, 1000 }) {
        for (int vps : { 1, 2 }) {
            s_results.push_back(
                runBenchmark(QStringLiteral("高密度多节点损坏 (10% 节点)"),
                             QStringLiteral("10% nodes concurrently dirty"),
                             nodes,
                             vps,
                             getIterations(nodes),
                             [](WSGDamageTracker &,
                                WSGDamageNode *,
                                const std::vector<WSGDamageGeometryNode *> &geos,
                                const std::vector<WSGDamageTransformNode *> &,
                                const std::vector<WSGDamageGeometryNode *> &,
                                int iter) {
                                 const size_t dirtyCount = std::max<size_t>(1, geos.size() / 10);
                                 for (size_t k = 0; k < dirtyCount; ++k) {
                                     const size_t idx = (iter + k * 7) % geos.size();
                                     geos[idx]->markContentDirty(QRect(5, 5, 20, 20));
                                 }
                             }));
        }
    }

    // 7. Hidden subtree receives changes that must be deferred until reveal.
    for (int nodes : { 1000, 5000 }) {
        s_results.push_back(runBenchmark(QStringLiteral("隐藏子树延迟更新"),
                                         QStringLiteral("Hidden root with one dirty descendant"),
                                         nodes,
                                         1,
                                         getIterations(nodes),
                                         [](WSGDamageTracker &,
                                            WSGDamageNode *root,
                                            const std::vector<WSGDamageGeometryNode *> &geos,
                                            const std::vector<WSGDamageTransformNode *> &,
                                            const std::vector<WSGDamageGeometryNode *> &,
                                            int iter) {
                                             root->setVisible(false);
                                             if (!geos.empty())
                                                 geos[iter % geos.size()]->markContentDirty(
                                                     QRect(10, 10, 16, 16));
                                         }));
    }

    // 8. Renderer replaces a very large QSG subtree with one ordinary damage proxy.
    constexpr int largeSubtreeNodes = 10000;
    s_results.push_back(runLargeSubtreeBenchmark(QStringLiteral("Exact large-subtree move"),
                                                 largeSubtreeNodes,
                                                 LargeSubtreeMode::ExactMove));
    s_results.push_back(runLargeSubtreeBenchmark(QStringLiteral("Renderer aggregate move"),
                                                 largeSubtreeNodes,
                                                 LargeSubtreeMode::RendererAggregateMove));
    s_results.push_back(runLargeSubtreeBenchmark(QStringLiteral("Exact descendant content"),
                                                 largeSubtreeNodes,
                                                 LargeSubtreeMode::ExactContent));
    s_results.push_back(
        runLargeSubtreeBenchmark(QStringLiteral("Renderer aggregate descendant content"),
                                 largeSubtreeNodes,
                                 LargeSubtreeMode::RendererAggregateContent));

    // Print Formatted Markdown Table
    printf("\n\n### 性能基准测试结果汇总 (Performance Benchmark Summary)\n\n");
    printf(
        "| 场景分类 (Scenario) | 节点数 (Nodes) | Viewport数 | 平均单帧耗时 (Avg Time) | "
        "最大理论吞吐量 (Throughput) | 60Hz 帧预算占比 | 144Hz 帧预算占比 | 240Hz 帧预算占比 |\n");
    printf("|---|---|---|---|---|---|---|---|\n");
    for (const auto &r : s_results) {
        printf("| %s | %d | %d VP | **%.2f µs** | %.0f FPS | %.3f%% | %.3f%% | %.3f%% |\n",
               r.category.toUtf8().constData(),
               r.nodeCount,
               r.viewportCount,
               r.avgTimeUs,
               r.maxFps,
               r.budget60Hz,
               r.budget144Hz,
               r.budget240Hz);
    }
    printf("\n");

    for (const auto &r : s_results) {
        const double limitUs = r.nodeCount >= 10000 ? 250000.0
            : r.nodeCount >= 5000                   ? 50000.0
                                                    : 16666.0;
        QVERIFY2(r.avgTimeUs < limitUs,
                 qPrintable(QStringLiteral("%1 nodes=%2 vps=%3 avg=%4 us exceeds %5 us budget")
                                .arg(r.scenario)
                                .arg(r.nodeCount)
                                .arg(r.viewportCount)
                                .arg(r.avgTimeUs, 0, 'f', 2)
                                .arg(limitUs, 0, 'f', 0)));
    }
}

QTEST_MAIN(WSGDamageGraphBench)
#include "bench_wdamagegraph.moc"
