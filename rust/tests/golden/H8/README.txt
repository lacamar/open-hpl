H8 golden vectors: cRenderList::AddObject decisions and the iRenderer::GetClipRectFromObject padding. Recorded from the C++ path.

list_<seed>.txt  cRenderList over stub iRenderables (8 render types, 4 material types, random BVs/matrices/frame counts), 3 frames:
                 "A" lines = one AddObject call: inputs (render type, material/type/translucent/decal/illum flags, occlusion query,
                 illumination, BV min/max/center, frustum origin, view matrix, frame counts) | observed gates, view-Z bits, list-membership mask.
                 Other lines: final list contents, Compile() sort orders, per-object view Z/frame counts/matrix pointer, material frame counts.
clip_<seed>.txt  2000 rows each: rect, screen, tan(fov/2) bits | padded rect (formula copied from Renderer.cpp).
Equal for OHPL_IMPL=cpp and rust on seeds 0-299 (list, clip); cargo replays "A" and clip rows (render::h8_tests) without the C++.

Record (needs a build dir of this tree with `make HPL2`):
  harness/gen.sh <build dir> h8_gen.cpp <out>/h8_gen
  harness/compare.sh <out>/h8_gen 300 <dir>
