#ifndef WW3D2_VULKAN_VK_SHADOW_PASS_H
#define WW3D2_VULKAN_VK_SHADOW_PASS_H

#if defined(RENEGADE_VULKAN)

#include <cstdint>

class DX8VertexBufferClass;
class DX8IndexBufferClass;
struct Matrix4;

namespace ww3d_vulkan {

/*
 * Native Vulkan volumetric shadow pass.
 * Bypasses the DX8 render-state → MeshPipelineKey bridge used by Try_Draw_Indexed.
 *
 * Default: dual-pass (original D3D8):
 *   Pass 1: D3DCULL_CW face set + stencil op (Y-flip compensated)
 *   Pass 2: D3DCULL_CCW face set + opposite op
 *
 * Env:
 *   GENERALS_SHADOW_TWO_SIDED=1     — one-draw two-sided stencil
 *   GENERALS_SHADOW_SWAP_FACES=0    — disable Y-flip stencil op swap
 *   GENERALS_SHADOW_SKIP_VOLUME=1   — skip volume draws (debug)
 */

enum class ShadowVolumeMode : uint8_t {
	Increment = 0, /* D3DCULL_CW + D3DSTENCILOP_INCR (dual-pass) */
	Decrement = 1  /* D3DCULL_CCW + D3DSTENCILOP_DECRSAT (dual-pass) */
};

bool Shadow_Volume_Pass_Active();
bool Shadow_Volume_Pass_Is_Two_Sided();

void Shadow_Volume_Pass_Begin(uint32_t stencil_shadow_mask);
void Shadow_Volume_Pass_Set_Mode(ShadowVolumeMode mode);
void Shadow_Volume_Pass_End();

/*
 * Draw one shadow volume mesh.  world_logical is the matrix that would be
 * passed to DX8Wrapper::Set_Transform(D3DTS_WORLD, ...) (already transposed
 * from Matrix3D as in the original RenderMeshVolume path).
 * vertex_offset is the D3D8 BaseVertexIndex (Set_Index_Buffer second arg).
 */
bool Shadow_Volume_Draw(
	DX8VertexBufferClass *vb,
	DX8IndexBufferClass *ib,
	unsigned start_index,
	unsigned polygon_count,
	unsigned vertex_offset,
	unsigned vertex_count,
	const Matrix4 &world_logical);

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */

#endif /* WW3D2_VULKAN_VK_SHADOW_PASS_H */
