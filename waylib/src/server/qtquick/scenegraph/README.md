# WSGBatchRenderer

Vendored fork of Qt Quick's `QSGBatchRenderer`.

## Source

- Tree: `/home/zccrs/projects/qtdeclarative`
- Tag: `v6.11.1` (base; version-gated to also build against 6.8.0 and 6.12.x)
- Commit: `a02bed441965ee1f18f856352c7d5ee5ba35d795`
- Upstream files:
  - `src/quick/scenegraph/coreapi/qsgbatchrenderer.cpp`
  - `src/quick/scenegraph/coreapi/qsgbatchrenderer_p.h`
  - `src/quick/scenegraph/coreapi/qsgrhivisualizer.cpp`
  - `src/quick/scenegraph/coreapi/qsgrhivisualizer_p.h`

## Local changes vs upstream

- Namespace `QSGBatchRenderer` renamed to `WSGBatchRenderer` to avoid clashing with `Q_QUICK_EXPORT` symbols in Qt6Quick.
- File and include-guard names renamed to `wsg*`.
- `qt_sg_envInt` inlined (not exported from Qt).
- `Q_QUICK_EXPORT` dropped from `Renderer` (same DSO as waylibserver).
- `QSGNode::m_subtreeRenderableCount` read via waylib's private accessor (Qt friends `QSGBatchRenderer::Renderer` only).
- Public helpers on `Renderer`: `create()`, `usesDepthBuffer()`, `shaderManager()`.
- `QSGSamplerDescription` compare/hash/`fromTexture` copied locally (Qt does not export them).
- Version gates so one copy builds against Qt 6.8.0, 6.11.x and 6.12.x:
  - 6.8.0: one-arg `fullDynamicBufferUpdateForCurrentFrame`
  - 6.9.2+: `invertFrontFace()` for layer culling
  - 6.12+: `QSGNodePrivate::mutabilityGroup` for batch compatibility
- `WSGContext` covers `QQuickRenderControlPrivate::sg` before the first
  `QQuickRenderControl` is constructed (`WOutputRenderWindow` and
  `WRenderHelper::getGraphicsApi()`). Its render context overrides
  `createRenderer()` so the window renderer is created as `WSGBatchRenderer`
  in `syncSceneGraph()`, instead of swapping Qt's renderer after `sync()`.
  If `QSGContext::createDefaultContext()` is not a `QSGDefaultContext`
  (software or another adaptation), that context is kept as `sg`.
- Extra sources / `WRenderBufferNode` create renderers via `rc->createRenderer()`.
- Damage tracking is not in this step; do not strip opaque/alpha/depth here either.

## Upgrade

When bumping the base Qt, three-way merge these four files against the recorded commit, then re-apply the rename, accessors, and `QT_VERSION_CHECK` gates.
