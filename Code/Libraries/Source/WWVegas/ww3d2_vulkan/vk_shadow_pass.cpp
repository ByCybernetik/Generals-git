#include "vk_shadow_pass.h"

#if defined(RENEGADE_VULKAN)

#include "vk_dx8_bridge.h"
#include "vk_dx8_state.h"
#include "ww3d_vulkan.h"
#include "renderdoc_capture.h"
#include "../ww3d2/dx8wrapper.h"
#include "../ww3d2/dx8vertexbuffer.h"
#include "../ww3d2/dx8indexbuffer.h"
#include "../ww3d2/dx8fvf.h"
#include "../wwmath/matrix4.h"
#include <cstdio>
#include <cstdlib>
#include <d3d8.h>

namespace ww3d_vulkan {

static bool g_shadow_pass_active = false;
static ShadowVolumeMode g_shadow_mode = ShadowVolumeMode::Increment;
static MeshPipelineKey g_key_two_sided;
static MeshPipelineKey g_key_incr;
static MeshPipelineKey g_key_decr;
static int g_native_draws = 0;
static bool g_use_two_sided = false;
/* Default OFF: classic D3D INCR→DECRSAT on the D3D cull face sets.
 * Set GENERALS_SHADOW_SWAP_FACES=1 if shadows are inverted under Y-flip. */
static bool g_yflip_swap = false;

static bool Env_Flag(const char *primary, const char *alias, bool default_on)
{
	const char *e = getenv(primary);
	if (e == nullptr) {
		e = getenv(alias);
	}
	if (e == nullptr) {
		return default_on;
	}
	if (e[0] == '0') {
		return false;
	}
	if (e[0] == '1') {
		return true;
	}
	return default_on;
}

static MeshPipelineKey Make_Volume_Key_Base(uint32_t stencil_mask)
{
	MeshPipelineKey key;
	key.terrain_shader = false;
	key.alpha_blend = false;
	key.src_blend = 1;
	key.dst_blend = 0;
	key.depth_write = false;
	key.depth_test = true;
	/*
	 * LESS (not LEQUAL): coplanar volume/terrain samples consistently fail
	 * for both front and back → no stripe residual. Soft contact may leave a
	 * 1px gap; D32 depth + small bias compensates.
	 * Override: GENERALS_SHADOW_VOLUME_DEPTH=lequal|always
	 */
	key.depth_compare = 1; /* LESS */
	{
		const char *d = getenv("GENERALS_SHADOW_VOLUME_DEPTH");
		if (d == nullptr) {
			d = getenv("SHADOW_VOLUME_DEPTH");
		}
		if (d != nullptr) {
			if (d[0] == 'a' || d[0] == 'A') {
				key.depth_compare = 7; /* ALWAYS — debug only, over-darkens */
			} else if ((d[0] == 'l' || d[0] == 'L') &&
				(d[1] == 'e' || d[1] == 'E') &&
				(d[2] == 'q' || d[2] == 'Q')) {
				key.depth_compare = 3; /* LEQUAL */
			}
		}
	}
	key.depth_clamp = false;
	key.depth_bias_enable = true;
	key.alpha_test = false;
	key.wireframe = false;
	key.topology = 0;
	key.fvf = DX8_FVF_XYZ;
	key.vertex_stride = 12;

	key.stencil_test = true;
	if (stencil_mask == 0x80808080u) {
		key.stencil_func = (uint8_t)D3DCMP_NOTEQUAL;
	} else {
		key.stencil_func = (uint8_t)D3DCMP_GREATEREQUAL;
	}
	key.stencil_ref = 0x80808080u;
	key.stencil_mask = stencil_mask;
	key.stencil_write_mask = 0xffffffffu;
	key.stencil_fail = (uint8_t)D3DSTENCILOP_KEEP;
	key.stencil_zfail = (uint8_t)D3DSTENCILOP_KEEP;

	key.color_write_mask = 0;
	return key;
}

static MeshPipelineKey Make_Dual_Pass_Key(
	uint32_t stencil_mask, bool cull_inverted, uint8_t stencil_pass_op)
{
	MeshPipelineKey key = Make_Volume_Key_Base(stencil_mask);
	key.two_sided = false;
	key.two_sided_stencil = false;
	key.cull_inverted = cull_inverted;
	key.stencil_pass = stencil_pass_op;
	key.stencil_pass_back = stencil_pass_op;
	return key;
}

static MeshPipelineKey Make_Two_Sided_Volume_Key(uint32_t stencil_mask)
{
	MeshPipelineKey key = Make_Volume_Key_Base(stencil_mask);
	key.two_sided = true;
	key.cull_inverted = false;
	key.two_sided_stencil = true;
	if (g_yflip_swap) {
		key.stencil_pass = (uint8_t)D3DSTENCILOP_DECRSAT;
		key.stencil_pass_back = (uint8_t)D3DSTENCILOP_INCR;
	} else {
		key.stencil_pass = (uint8_t)D3DSTENCILOP_INCR;
		key.stencil_pass_back = (uint8_t)D3DSTENCILOP_DECRSAT;
	}
	return key;
}

static void Build_Keys(uint32_t stencil_mask)
{
	if (g_yflip_swap) {
		g_key_incr = Make_Dual_Pass_Key(
			stencil_mask, false, (uint8_t)D3DSTENCILOP_DECRSAT);
		g_key_decr = Make_Dual_Pass_Key(
			stencil_mask, true, (uint8_t)D3DSTENCILOP_INCR);
	} else {
		g_key_incr = Make_Dual_Pass_Key(
			stencil_mask, false, (uint8_t)D3DSTENCILOP_INCR);
		g_key_decr = Make_Dual_Pass_Key(
			stencil_mask, true, (uint8_t)D3DSTENCILOP_DECRSAT);
	}
	g_key_two_sided = Make_Two_Sided_Volume_Key(stencil_mask);
}

bool Shadow_Volume_Pass_Active()
{
	return g_shadow_pass_active;
}

bool Shadow_Volume_Pass_Is_Two_Sided()
{
	return g_shadow_pass_active && g_use_two_sided;
}

void Shadow_Volume_Pass_Begin(uint32_t stencil_shadow_mask)
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		g_shadow_pass_active = false;
		return;
	}

	RenderDoc_Trigger_Once();

	VkRenderer &renderer = WW3DVulkan::Get().Renderer();
	if (renderer.Frame_Active()) {
		renderer.Flush_Pending_Draws();
	}

	g_use_two_sided = Env_Flag(
		"GENERALS_SHADOW_TWO_SIDED", "SHADOW_TWO_SIDED", false);
	if (!g_use_two_sided) {
		g_use_two_sided = Env_Flag("TWO_SIDED", "TWOSIDED", false);
	}

	g_yflip_swap = Env_Flag(
		"GENERALS_SHADOW_SWAP_FACES", "SHADOW_SWAP_FACES", false);
	if (getenv("SWAP_FACES") != nullptr) {
		g_yflip_swap = Env_Flag("SWAP_FACES", "SWAP_FACES", false);
	}

	Build_Keys(stencil_shadow_mask);
	g_shadow_mode = ShadowVolumeMode::Increment;
	g_shadow_pass_active = true;
	g_native_draws = 0;

	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(1, nullptr);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);
	DX8Wrapper::Set_DX8_ZBias(0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZBIAS, 0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESS);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
	Sync_Draw_State();
}

void Shadow_Volume_Pass_Set_Mode(ShadowVolumeMode mode)
{
	if (!g_shadow_pass_active || g_use_two_sided) {
		return;
	}
	g_shadow_mode = mode;
}

void Shadow_Volume_Pass_End()
{
	if (!g_shadow_pass_active) {
		return;
	}
	g_shadow_pass_active = false;
}

bool Shadow_Volume_Draw(
	DX8VertexBufferClass *vb,
	DX8IndexBufferClass *ib,
	unsigned start_index,
	unsigned polygon_count,
	unsigned vertex_offset,
	unsigned vertex_count,
	const Matrix4 &world_logical)
{
	(void)vertex_count;
	if (!g_shadow_pass_active || !Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return false;
	}
	if (vb == nullptr || ib == nullptr || polygon_count == 0) {
		return false;
	}

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	{
		static int skip_vol_env = -1;
		if (skip_vol_env < 0) {
			const char *e = getenv("GENERALS_SHADOW_SKIP_VOLUME");
			if (e == nullptr) {
				e = getenv("SKIP_VOLUME");
			}
			skip_vol_env = (e != nullptr && e[0] == '1') ? 1 : 0;
		}
		if (skip_vol_env) {
			return true;
		}
	}
#endif

	VkBuffer vkb = VB_Handle(vb);
	VkBuffer ikb = IB_Handle(ib);
	if (vkb == VK_NULL_HANDLE || ikb == VK_NULL_HANDLE) {
		return false;
	}

	Matrix4 view;
	Matrix4 projection;
	DX8Wrapper::Get_Transform(D3DTS_VIEW, view);
	DX8Wrapper::Get_Transform(D3DTS_PROJECTION, projection);
	Sync_Matrices(world_logical, view, projection);

	const MeshPipelineKey *key = &g_key_two_sided;
	if (!g_use_two_sided) {
		key = (g_shadow_mode == ShadowVolumeMode::Increment) ? &g_key_incr
															 : &g_key_decr;
	}

	WW3DVulkan::Get().Draw_Indexed(
		vkb,
		ikb,
		polygon_count * 3u,
		start_index,
		vertex_offset,
		*key);

	++g_native_draws;
	return true;
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
