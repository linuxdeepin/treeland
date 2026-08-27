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
- `WSGBatchRenderer` maps the output QSG tree onto `WSGDamageNode`s
  (`Geometry` / `Transform` / `Clip`; glass is Geometry + `needsBackdrop`).
  The compositor output pass runs the three-phase tracker
  (`prepareFrame` / `commit` / `finishFrame`). `flushRegion()` is GPU
  scissors (`sceneFlush ∪ bufferAge`). `sceneFlushRegion()` is the ring
  write-back (actual RT writes from the shared `accumulateFlush` walk).
  Glass recapture is this output's flush accumulator ∩ glass bounds
  (`accumulateFlush` copy source), then kernel dilation. Do not use
  Tracker `behindDamage` — that is world-space and not per-viewport.
  Nested QSGLayer / MultiEffect passes that reuse this Renderer do not
  run the three-phase tracker or prepare/render blitters.
  `WSGDamageClipNode` clips descendant world bounds, opaque, and damage
  in world space so a ListView scroll reports the viewport. Geometry
  keeps unclipped local bounds; a non-invertible transform does not
  need a clip inverse. Local bounds prefer a typed rect
  (`QSGImageNode` / `QSGRectangleNode` / `QSGSimpleRectNode` /
  `QSGSimpleTextureNode` / QML internal rectangle and image /
  nine-patch / glyph / painted-item / `QSGClipNode::clipRect`) and
  fall back to a 2D vertex AABB. Clip nodes with an empty `clipRect`
  use that vertex AABB. `QQuickDefaultClipNode` copies `rect()` and
  `radius()`: damage still uses the AABB, opaque is the AABB minus
  `ceil(radius)` corner squares. Unknown stencil clips
  (`!isRectangular()` and radius 0) contribute no descendant opaque.
  QML `Rectangle` fill opaque uses the same inner region when radius > 0.
  `DirtyMaterial` without
  `WSGImageNode` explicit damage is ignored (image-node polish must not
  recopy every blitter). Detached nested `RootNode`s are ignored via
  the pinned tracked root.
  With `QT_LOGGING_RULES=waylib.renderer.damage.debug=true` (no extra quotes),
  each compositor output frame is wrapped by `======== [fN] BEGIN ========`
  and `======== [fN] END ========`. Blitter lines start with `blitter <name>`.
  `will recapture` vs `keep last capture` is the prepare decision;
  `draw: recapture` copies compositor pixels under the blitter,
  `draw: reuse last capture` composites the last capture because pending
  punches the blitter; idle blitters are listed once at prepare end.
  The extra-QRhi renderer used to draw content / rotate-blit has damage
  tracking off.
- Recycled swapchain buffers `PreserveColorContents`. `DontCare` resolves
  to Preserve (wlroots `LOAD`), so a buffer's RT is not flipped
  Clear↔Preserve. GLES and Vulkan both follow wlr_scene: keep the
  recycled pixels and redraw `flushRegion` under GPU scissors (one scissor
  + draw per rect, capped like `WLR_DAMAGE_RING_MAX_RECTS`). Swapchain
  buffer damage from `wlr_damage_ring_rotate_buffer` is `setViewportDamage`
  on the next output pass. GPU scissors are `flush ∪ bufferAge`. It is
  **not** written back to the damage ring —
  `sceneFlushRegion()` is scene-graph flush only, or slots ping-pong the
  same rects. Nested `QSGLayer` / `MultiEffect` offscreen RTs reuse the
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
  old and new full bounds. The same node carries `wl_surface` opaque_region
  (inner-mapped into item space). Empty explicit opaque means the client
  claimed no opaque pixels; do not fall back to material blending.
  `QRegion::contains(QRect)` means overlap, not
  coverage; a caret-sized dirty region must not collapse to a full output
  flush. Cover the output with `QRegion(output).subtracted(damage).isEmpty()`.
- `WAYLIB_DAMAGE` is read by `WSGBatchRenderer::Renderer` (default `on`):
  - `off` / `0` / `none`: no tracker. Draw and present like the pre-damage
    batch renderer (`e848551`). `wlr_damage_ring_add_whole`.
  - `commit` / `output`: track and report `sceneFlushRegion` to the
    output (`wlr_damage_ring`). Draw is still fullscreen (no scissor, no
    cull, blitters recopy). `flushRegion` is GPU scissors only
    (`sceneFlush ∪ bufferAge`) and is not written to the ring.
  - `on` / `full`: track, GPU scissor, cull, and recopy blitters by damage.
  `Renderer::setDamageTrackingEnabled(false)` forces `off` on one renderer
  (nested MultiEffect / extra-QRhi).
  Visual overlay is `WOutputRenderWindow::damageVisual`
  (`WAYLIB_DEBUG_DAMAGE=highlight`; default off).
  Per-step tracker / blitter logs are independent:
  `QT_LOGGING_RULES=waylib.renderer.damage.debug=true`.
  Highlight is a `WOutputLayer` created per viewport by
  `WOutputRenderWindow`. Quads are pooled `QQuickRectangle`s; the layer
  is sized to the overlay AABB. `OutputHelper` records `lastFlushRegion`
  after each output render. Batch renderer and `WBufferRenderer` do not
  own overlay state.
  Nested `QSGLayer` / `grabToImage` / `MultiEffect` / blitter passes are
  unaffected.
  - `off` (default): layer disabled
  - `highlight` (or `1` / `true`): overlay of each frame's `flushRegion`
    on the dedicated layer. Current frame is red; older live frames shift
    toward green. Alpha fades over 250ms.
  The debug menu **Damage** entry (Ctrl+Shift+Meta+F11) switches highlight
  on or off at runtime.

## Testing damage

`waylib/tests/unit_tests/test_wsgdamage` has two layers:

1. Overlay / blitter tests in `main.cpp` (`WRenderBufferNode::applyFrame`
   / `needsRender`, highlight fade). Graph math lives in `test_wdamagegraph`.
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
4. Replay those entries with `WSGDamageOverlay::paint` onto the grab.
5. Compare the committed buffer with the painted grab, and also compare
   dump vs grab *outside* the overlay (leftover 2px border on wallpaper
   fails this).

Also covers fade frames while highlight stays on, idle compositor frames
(`OutputRenderWindow::render()` without `forceRender` — this is the path
that used to skip overlay fade when the QML scene was not dirty), and
software-cursor moves driven by `WCursor::setPosition`. `frosted_behindMoveMatchesRerender`
turns on a
`RenderBufferBlitter` over the photo (same setup as `waylib/examples/blur`)
and compares the blitter pixels after a behind-item move against a full
redraw of the same scene — not `grabToImage`, which cannot see the
compositor copy.

`LockScene.qml` is a replica of `treeland --lockscreen` (UserInput +
session chip + SessionList). It uses Qt Quick Controls and waylib
`RenderBufferBlitter` (`RoundBlur.qml`), not DTK, so waylib-only CI
can build it. It does not import compositor plugins. Visual tests
drive it through `showLockscreen()`:

- popup open: damage covers the popup, not fullscreen, not a
  full-output-height stripe
- list scroll: damage height is the list viewport (clip), and blitter
  pixels match a full redraw (no whiteboard; blur stays)
- caret blink: covers the caret, not the whole password field
- idle password blur: two frames match and do not recopy the blitter

The session `Popup` uses `popupType: Popup.Item` and is parented to the
lock scene (`modal: false`) so it sits in the output tree rather than
`QQuickOverlay`.

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


`waylib/tests/manual/damage` is a compositor playground for this damage
work. It turns on `WAYLIB_DEBUG_DAMAGE=highlight` unless the env var is
already set. Rotate, move and scale keep running; drag the opaque / alpha
/ blitter covers over them to see whether `flushRegion` follows the actor
AABB. `waylib/tests/manual/damagegraph-demo` is the same
`WSGDamageTracker` visualizer without a compositor (Debug builds only).
Its C++ (`demoscene`, overlay, visual model) is shared with
`waylib/tests/manual/damage`; only the QML window chrome differs.

```bash
cmake --build build -j160 --target damage damagegraph-demo
./build/waylib/tests/manual/damage/damage
./build/waylib/tests/manual/damagegraph-demo/damagegraph-demo
```

## Upgrade

When bumping the base Qt, three-way merge these four files against the recorded commit, then re-apply the rename, accessors, and `QT_VERSION_CHECK` gates.
