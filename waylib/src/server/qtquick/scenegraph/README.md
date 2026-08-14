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

- Namespace `QSGBatchRenderer` moved to `WSGBatchRenderer` under
  `WAYLIB_SERVER_BEGIN_NAMESPACE` (a native waylib type, not a Qt-namespace
  fork).
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
  `WRenderBufferNode` currently recopies the framebuffer and redraws its
  effect subtree on every visit. Damage-aware recopy will be a later
  refactor of that class. Renderer GPU scissors still open to the device
  rect around this node so a GLES copy is not clipped to a damage hole.
-   `WSGDamageTracker` (driven from `nodeChanged()`) unions dirty geometry into
  a region and exposes `flushRegion()` in scene-graph coordinates.
  `WBufferRenderer` maps that through the world transform and DPR onto
  `wlr_damage_ring` instead of `wlr_damage_ring_add_whole`. `DirtyNodeAdded`
  records the subtree AABB, including empty nodes (hidden Overlay). Later
  matrix/geometry unions that last AABB with the current one; a node that
  was never Added has no old pixels, so only the current AABB is reported
  (do not markFull — that was painting the whole output for Popup / hardware
  cursor). Empty `now` after a matrix change means the pixels left this tree
  (OutputLayer / hideSource) and is skipped. Opacity, subtree-blocked,
  remove, and `DirtyForceUpdate` use the live subtree AABB instead of
  full-output — a blinking text caret must not refresh the whole screen.
  The first frame still reports a full buffer, but child AABBs are cached
  during that frame so the next move/rotate is old∪new instead of another
  full output. Qt only sends `DirtyNodeAdded` for the inserted subtree root
  (`setRootNode`, or a parent appending an already-built `itemNode`), so the
  tracker walks that subtree and caches nested TransformNodes. Do not strip
  opaque/alpha/depth here.
- Recycled swapchain buffers `PreserveColorContents`. `DontCare` resolves
  to Preserve (wlroots `LOAD`), so a buffer's RT is not flipped
  Clear↔Preserve. GLES and Vulkan both follow wlr_scene: keep the
  recycled pixels and redraw `flushRegion ∪` ring-stale damage under GPU
  scissors (one scissor + draw per rect, capped like
  `WLR_DAMAGE_RING_MAX_RECTS`). Ring damage expands the scissor, not
  `flushRegion`. Nested `QSGLayer` / `MultiEffect` offscreen RTs reuse the
  same `Renderer`; damage scissors apply only while the output RT that
  `WBufferRenderer` just bound is the current target. There is no blit from
  the previous buffer. A fully dirty
  buffer is redrawn without scissor. Damage bounds always use the current
  TransformNode stack, not the last-render `QSGGeometryNode::matrix()`
  (that pointer is stale during `nodeChanged` and would miss the new cursor
  position).
- `WSGImageNode` (from `WSGContext::createImageNode`) carries optional
  content damage on the image node itself (e.g. `WSurfaceItemContent` +
  `wlr_surface_get_effective_damage`). `DirtyMaterial` on that node reports
  only the mapped explicit region (empty = no content damage). Add/move/
  resize/remove still use the geometry AABB so a window moving dirties the
  old and new full bounds.
- `WAYLIB_DEBUG_DAMAGE` mirrors wlroots `WLR_SCENE_DEBUG_DAMAGE`:
  Highlight draws scene-space quads through `projectionMatrix()` (the same
  Y-up clip space as scene batches). The overlay pipeline scissors to the
  union of those quads in native framebuffer space (`sceneRectToNativeScissor`)
  so Vulkan cannot keep the last scene-batch dynamic scissor. Mapping failure
  falls back to the device rect — not the output damage AABB.
  Nested `QSGLayer` / `grabToImage` / `MultiEffect` renders reuse this
  `Renderer`; damage scissors apply only while the current RT is the output
  target `WBufferRenderer` bound.
  - `none` (default): off. Switching from highlight still full-redraws several
    frames so leftover overlay is erased from every recycled swapchain image
    (`grabToImage` also switches to None and would otherwise clear leftover
    after dirtying only the current extra region).
  - `rerender`: force a full redraw every frame
  - `highlight` (or `1` / `true`): overlay of each frame's `flushRegion`.
    The current frame is red; older frames that have not timed out shift
    toward green (one hue per live frame). Alpha still fades out over
    250ms. Fading rects are re-damaged until they expire; that must mark the
    output content dirty (not only `wlr_output_schedule_frame`), otherwise
    `doRenderOutputs` skips the redraw and the overlay stays in recycled
    swapchain buffers. Overlay is only
    drawn on `WBufferRenderer` output buffers, not offscreen blitters.
  - `log`: print each frame's `flushRegion` (`full` or `WxH+X+Y,...`) at
    `waylib.render.bufferrenderer` info. The debug menu **Damage** entry
    (Ctrl+Shift+Meta+F11) can switch these modes at runtime.

## Testing damage

`waylib/tests/unit_tests/test_wsgdamage` has two layers:

1. Node-level tracker tests in `main.cpp` (add/move/remove/material/image-node
   damage, per-node `damageForNode` / `addToFlush`).
2. QML scene tests in `scenetest.cpp`. They install `WSGContext`, drive an
   offscreen `QQuickRenderControl` window, `polishItems()`+`sync()`, then
   `Renderer::commitPendingDamage()` and assert `flushRegion`:
   - add / remove / move / rotate / scale / `Translate` transform
   - caret opacity blink must not cover distant buttons
   - idle frames stay empty

Failures dump the actual region (`WxH+X+Y`). To add a scenario: put the
widgets in `kSceneQml`, mutate one item, `take()`, and assert covers/avoids
the mapped scene rects.

`test_wsgdamage_visual` covers DamageDebug overlay pixels. It starts a
headless compositor, loads `TestWindow.qml` / `DamageScene.qml` (the same
photo as `examples/test_glass`, rounded card + shadow, MultiEffect blur of
the photo, and an optional `RenderBufferBlitter` frosted-glass panel), then
for each frame:

1. On `afterRendering`, snapshot `flushRegion` and highlight entries.
   Dump the `wlr_buffer` to `QImage` only after `render()` returns
   (`QQuickRenderControl::endFrame` has submitted the Vulkan commands).
2. Assert `flushRegion` covers the expected item bounds (including shadow
   / blur padding) and misses a distant sentinel.
3. `grabToImage` the QML scene on a follow-up frame with highlight off
   (the grab itself refs the item as an effect source and would otherwise
   dirty this frame's region).
4. Replay those entries with `WSGDamageDebug::paint` onto the grab.
5. Compare the committed buffer with the painted grab, and also compare
   dump vs grab *outside* the overlay (leftover 2px border on wallpaper
   fails this).

Also covers fade frames while highlight stays on, idle compositor frames
(`OutputRenderWindow::render()` without `forceRender` — this is the path
that used to skip overlay fade when the QML scene was not dirty),
software-cursor moves driven by `WCursor::setPosition`, and rerender
(no overlay). `frosted_behindMoveMatchesRerender` turns on a
`RenderBufferBlitter` over the photo (same setup as `waylib/examples/blur`)
and compares the glass pixels after a behind-item move against a full
`rerender` of the same scene — not `grabToImage`, which cannot see the
compositor copy.

Run with:

```bash
cmake --build build -j160 --target test_wsgdamage test_wsgdamage_visual
ctest --test-dir build --output-on-failure -R test_wsgdamage
```

With no extra env vars the binaries spawn child runs themselves:
`test_wsgdamage` uses `QSG_RHI_BACKEND=null`, `opengl`, then `vulkan`
(offscreen Vulkan skips the QML scene tests — no `QVulkanInstance`).
`test_wsgdamage_visual` uses `WLR_RENDERER=gles2` then `vulkan`.
Set those variables yourself to run a single backend.
Vulkan still runs region checks, but skips dump-vs-`grabToImage`
comparisons that include live `MultiEffect` (blur / drop shadow).


`waylib/examples/damage` is a manual compositor playground. It turns on
`WAYLIB_DEBUG_DAMAGE=highlight` unless the env var is already set. Rotate,
move and scale keep running; drag the opaque / alpha / glass covers over
them to see whether `flushRegion` follows the actor AABB.

```bash
cmake --build build -j160 --target damage
./build/waylib/examples/damage/damage
```

## Upgrade

When bumping the base Qt, three-way merge these four files against the recorded commit, then re-apply the rename, accessors, and `QT_VERSION_CHECK` gates.
