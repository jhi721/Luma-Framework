#define GAME_YAKUZA_REMASTERED_COLLECTION 1

#define DISABLE_AUTO_DEBUGGER 1
// SMAA replaces the game's CMAA2 or FXAA (see "RunSMAA").
#define ENABLE_SMAA 1
// Luma Bloom replaces the glow pyramid (see "RunLumaBloom").
#define ENABLE_BLOOM 1
// XeGTAO and Luma Bloom re-issue the game's ASSAO apply and glow_pass2 draws with their own pixel shaders, so they need
// "original_draw_dispatch_func" (see "DrawWithLumaPixelShader").
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// Effects that relied on the vanilla UNORM clamp get a saturate appended in place, and the Y5R material tone curve is
// extended in place (see "PatchShaderBytecodeSync").
#define LUMA_PATCH_BYTECODE_SYNC 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"

// The engine has no tone curve: materials write gamma-space `color * exposure` into an 8-bit scene RT whose UNORM clamp
// is the only highlight limit (Y5R materials add their own curve, see "PatchY5MaterialToneCurve"). The whole post chain
// (scene, CMAA2, CAS, resample, fade) runs on scene-sized b8g8r8a8/r8g8b8a8 targets, upgraded to fp16 here. The HDR
// tonemap lives in the "color correct" replacements (Shaders/Yakuza Remastered Collection/Includes/ColorCorrect.hlsl),
// the first full-screen pass reading the finished scene.
// Hashes are Y3R's unless marked. A hash absent from the running game never matches, so the games' sets are merged;
// only what is not keyed by a unique hash lives in the game profile.

namespace
{
   struct YakuzaGameProfile
   {
      const char* name;
      std::vector<uint2> dof_custom_sizes;         // Offscreen DoF targets upgraded to fp16 besides the scene-sized ones
      uint32_t glow_downsample_pixel_shader;       // Flagged to clamp its source per texel; per game because Y3's 0x54A5E7AC is Y4's ps_cubic
      bool grades_aliased_passthrough_ccr = false; // Y5R: its passthrough ccr is byte-identical to ps_texture_a255 (see below)
      bool glow_downsample_rms = false;            // Y4R's downsample is a 5x6-tap root mean square, Y3R/Y5R's a 2-tap average
      bool material_tone_curve = false;            // Y5R: lit materials tone-compress in-shader (see "PatchY5MaterialToneCurve")
      bool reversed_depth = false;                 // Y4R/Y5R: reversed Z (sky = 0), ASSAO unpacks it as -0.1 / (-0.0001 - d)
      float glow_level0_sigma = 1.f;               // Luma Bloom's first blur (1024 prefilter -> 512x256 level 0), see bloom_level_sigma
      uint32_t material_scale_register = 14;       // The materials' PS cb2 exposure scale (cb2[14]; Y5R cb2[15]), DEV logs
   };
   YakuzaGameProfile g_game_profile; // Selected once in DllMain

   // Y4 adds a 1024x1024 step to the DoF scene copy (4K -> 1024 -> 512), Y5 a 256x256 blur level. Y5 downsamples the glow
   // source twice per frame; the second one (listed) feeds the bloom. An unknown executable is treated as Y3.
   YakuzaGameProfile DetectGame()
   {
      std::string exe = System::GetProcessExecutableName();
      for (auto& c : exe)
         c = (char)tolower((unsigned char)c);
      if (exe.find("yakuza5") != std::string::npos)
         return {.name = "Yakuza 5 Remastered", .dof_custom_sizes = {{512, 512}, {512, 256}, {256, 256}}, .glow_downsample_pixel_shader = 0x54D6A534, .grades_aliased_passthrough_ccr = true, .material_tone_curve = true, .reversed_depth = true, .glow_level0_sigma = 3.f, .material_scale_register = 15};
      if (exe.find("yakuza4") != std::string::npos)
         return {.name = "Yakuza 4 Remastered", .dof_custom_sizes = {{1024, 1024}, {512, 512}, {512, 256}}, .glow_downsample_pixel_shader = 0x66633BAD, .glow_downsample_rms = true, .reversed_depth = true};
      return {.name = "Yakuza 3 Remastered", .dof_custom_sizes = {{512, 512}, {512, 256}}, .glow_downsample_pixel_shader = 0x54A5E7AC};
   }

   // User settings, persisted in the [Luma] config section.
   bool g_smaa_enable = true;
   float g_rcas_sharpness = 0.f;
   // Plane deviation that counts as a full predication edge, as a fraction of view depth (TW2's validated value; DEV
   // slider, not persisted).
   float g_smaa_pred_tolerance = 0.02f;
   bool g_gtao_enable = true;
   bool g_luma_bloom_enable = true;
   float g_gtao_final_value_power = 1.f; // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;   // > 0 overrides the shader's EFFECT_RADIUS (ASSAO's own radius, view units)
   bool g_hide_ui = false;               // Session-only, so a restart never comes back without a HUD
#if DEVELOPMENT
   bool g_smaa_predication = true;
   bool g_smaa_pred_debug = false;   // Show the predication mask (red) instead of the frame
   bool g_smaa_pred_measure = false; // One-shot: read the mask back and log its distribution (UI button)
   int g_gtao_debug_view = 0;        // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
   bool g_gtao_skip_apply = false;   // Calibration: frame without any AO (native or XeGTAO)
#endif

   // Intel ASSAO (stock): prepare, depth mips, generate (High / Medium), smart blur / wide, all skipped under XeGTAO; the
   // apply multiply-blends the AO onto the scene mid material stream (see "RunXeGTAO").
   constexpr uint32_t assao_prepare_pixel_shader = 0x972BE5B5;
   const std::unordered_set<uint32_t> assao_pre_apply_pixel_shaders = {0x1DD919C4, 0x47BFF17F, 0xD18E0D3F, 0x8CE62D1E, 0x15EEFFAF};
   constexpr uint32_t assao_apply_pixel_shader = 0x6A73BA10;
   constexpr UINT gtao_knobs_cb_slot = 8; // "register(b8)" in Luma_YRC_XeGTAO.hlsl
   // The glow pyramid: glow_pass0 builds its 512x256 level, 8x glow_pass1 blur it down to 32x16, glow_pass2 sums the 5
   // levels onto the scene. Luma Bloom rebuilds the same 5 levels with the shared pyramid from a 1024x512 prefilter.
   const std::unordered_set<uint32_t> glow_pass0_pixel_shaders = {0xB8414674, 0xD31A6374 /*Y5R*/};
   constexpr uint32_t glow_pass1_pixel_shader = 0x9DD96515;
   const std::unordered_set<uint32_t> glow_pass2_pixel_shaders = {0x9083BF34, 0x9E617E0A /*Y4R*/, 0x5F37CDE1 /*Y5R*/};
   constexpr uint32_t glow_reexpand_pixel_shader = 0xFD753992;     // Y5R ps_down_sample_4x4 (see bloom_level_sigma)
   constexpr UINT glow_prefilter_width = YRC_GLOW_PREFILTER_WIDTH; // Includes/GameCBuffers.hlsl
   constexpr UINT glow_prefilter_height = YRC_GLOW_PREFILTER_HEIGHT;
   constexpr int bloom_mips = 5; // 512x256 down to 32x16, the vanilla levels
   // Vanilla glow_pass1 (Y3 DEV log): 6 taps at +-0.5/1.5/2.5 source texels, near-flat weights 0.1676/0.1671/0.1653
   // (sum 0.5 per side, unit gain) -> sigma 1.70 per axis per level. Level 0 has no pass1 blur, only the 2:1 antialias
   // (sigma 1), except in Y5R: after its exposure meter, ps_down_sample_4x4 0xFD753992 re-expands a 256x128 8-bit copy of
   // the level over itself before glow_pass0 (4 bilinear taps at +-VS cb7[0].xy = (1/512, 1/256), 0.5 source texel, DEV log):
   // sigma 3 fits a model of that chain.
   constexpr float bloom_level_sigma = 1.7f;
   constexpr UINT gtao_depth_mip_count = 5; // XE_GTAO_DEPTH_MIP_LEVELS in Luma_YRC_XeGTAO.hlsl
   // Readers of the 4K r32 scene depth at t0: the ASSAO prepare, and a full-screen depth restore (also used by prepasses;
   // the last one before the AA, after ASSAO, reads the main depth). Captured for SMAA predication.
   const std::unordered_set<uint32_t> depth_reader_pixel_shaders = {assao_prepare_pixel_shader, 0x87917E9E};
   // QLOC's CMAA2: edges, candidates, dispatch args (these three skipped under SMAA), then the apply, which writes edge
   // pixels into u0, a copy of the post-ccr canvas that the game copies back afterwards.
   constexpr uint32_t cmaa2_first_compute_shader = 0x2E998140;
   const std::unordered_set<uint32_t> cmaa2_pre_apply_compute_shaders = {cmaa2_first_compute_shader, 0x60E701EA, 0x79976116};
   constexpr uint32_t cmaa2_apply_compute_shader = 0x82DA801B;
   // FXAA 3.11: one pass from the post-ccr canvas (t0) into its own scene-sized target.
   constexpr uint32_t fxaa_pixel_shader = 0xE7A1D308;
   // The HUD: the exe's sprite shaders (Y3R/Y4R sys_modulatealpha_lanczos 0xBB31B137, ui2 0xCCF77328, 0x80C378A8; Y5R's
   // Lanczos 0x3EA58102 / 0x2C439929, solid fill 0x9B3FB6FA, circles 0x590EF3E9 / 0xECA886BD) and the world-anchored
   // markers and name tags (p_dvtx/p_nvtx, Y3R 0xD66497C5, Y4R/Y5R 0x3574C123), all drawn onto the scene-sized canvas
   // after the scene (the markers before CAS, the rest after the resample).
   const std::unordered_set<uint32_t> ui_pixel_shaders = {0xBB31B137, 0xCCF77328, 0x80C378A8, 0x3EA58102, 0x2C439929, 0x9B3FB6FA, 0x590EF3E9, 0xECA886BD, 0xD66497C5, 0x3574C123};
   // FidelityFX CAS (t0 -> u0, copied back by the game), after the AA and the world-anchored markers.
   constexpr uint32_t cas_compute_shader = 0x491BAFA3;
   // Its scaling variant, the resample to the swapchain when the render scale is not 100% (with CAS on).
   constexpr uint32_t cas_scaled_compute_shader = 0xFBA57AE9;
   // Y5R's passthrough ccr compiles byte-identical to ps_texture_a255, a copy with alpha 1 that every game uses (DoF
   // buffers, among others). Its replacement grades only the draws flagged here: scene-sized targets in Y5R.
   constexpr uint32_t aliased_passthrough_ccr_pixel_shader = 0x2DD46662;
   // Effects drawn into targets that were UNORM in vanilla (the scene RT, the DoF-sized offscreen buffers), which
   // relied on that clamp: particles (ps_ptc_*, blood included), the hit flashes, edges and highlight masks, shockwave,
   // aura, blood decals, body damage marks, the Y5R haze mask, star glare and DoF alpha passes. On the fp16 chain their
   // colors went far above 1 (glowing blood) and alphas above 1 extrapolated the blend (black and white streaks).
   // The water ripple simulation (fx_ripple's height field, fx_ripple2's impulses) runs in a 512x512 target that shares
   // the fp16 DoF size: the clamp keeps its heights in the vanilla range.
   // glow_pass1 (0x9DD96515, every game) is here too: the 512x256 bloom level shares the fp16 DoF size, and the clamp keeps
   // the vanilla bloom bound. The Y5R menu DoF blur2 (0x9BB484AC) divides by an alpha sum that can be 0: the clamp also
   // turns its NaN into 0, as the vanilla UNORM store did. Every one has a single o0.xyzw output and a single final ret
   // (checked on the disassembly); listed from the games' shader archives, the Y4R/Y5R recompiles of the same shaders last.
   const std::unordered_set<uint32_t> unorm_clamped_effect_pixel_shaders = {0x024B22FC, 0x098F82BB, 0x0E0E3FDE, 0x0EA64B7C, 0x1233B2C0, 0x1490E21C, 0x15DBE648, 0x17153683, 0x179EC828, 0x1BFC5407, 0x1C9CF72D, 0x1E7304D2, 0x21F9EE5F, 0x233DF244, 0x23F5C577, 0x262029A4, 0x2707F90E, 0x2A75EC72, 0x2C018858, 0x2E5C72EF, 0x3019A9B8, 0x308E9227, 0x3468253F, 0x3533A116, 0x378AD557, 0x3A25DD61, 0x3AD048E7, 0x3BB5AE11, 0x3CCC13A9, 0x412945F3, 0x47F353EE, 0x47F97A40, 0x4A4CBF32, 0x4F656839, 0x5475205C, 0x581526D2, 0x6011CF50, 0x66F39F82, 0x6772EAB4, 0x6DBDDBAD, 0x6DDEF9B7, 0x71DF2C06, 0x747526C6, 0x75F2BE3E, 0x79B54068, 0x7AA982CE, 0x810027FF, 0x8A8F73E0, 0x90BD986C, 0x9486446E, 0x9552AB8B, 0x96E88E1B, 0x9A735E6D, 0x9DD96515, 0xA38D13EC, 0xA845CFD6, 0xA8E99745, 0xAD1EACD9, 0xB1C465A7, 0xB2EEF041, 0xB795066D, 0xB820683B, 0xB97E0BA0, 0xBEA87CEF, 0xC3F1CC7A, 0xC77EF0DF, 0xCF0DF8B9, 0xD0DF2846, 0xD2CB4337, 0xD939FD47, 0xDF16C6DB, 0xF24C81DF, 0xF2FA9571, 0xF3B188D2, 0xFA77FFFE,
      /*Y4R*/ 0x0DEA8926, 0x13800646, 0x3E46D498, 0x46F3A3D5, 0x93E6D9B7, 0x9BEF0D68, 0xA5E4CDED, 0xA650D8FF,
      /*Y5R*/ 0x01CACE56, 0x03A09BCE, 0x04D2B65E, 0x08729A68, 0x0B3618F8, 0x12FE69CE, 0x22B445CB, 0x27C05456, 0x2A215145, 0x2DF42E11, 0x32895C4E, 0x39AE54DB, 0x39F4C5F7, 0x3B0A6B3A,
      0x41CF9297, 0x520829D8, 0x5478E175, 0x5516C25C, 0x5C6FE9C1, 0x5CAB83C7, 0x5EA728DE, 0x6137472C, 0x61F61EFA, 0x6318F86D, 0x6A85F04B, 0x72A43180, 0x7B6B2E75, 0x7DFE4BEA,
      0x7FEC9B44, 0x8083C110, 0x82552855, 0x85D925A4, 0x8C90CF92, 0x92627902, 0x9AC1CDB0, 0x9BED49EA, 0xA156C6FA, 0xABB7BCA4, 0xAE62C507, 0xB2EA0E19, 0xB4C297E5, 0xB620DDFB,
      0xB68688F9, 0xB68FA494, 0xB78A1D16, 0xBA8283E5, 0xBB2C0F00, 0xC35158E3, 0xD0BEFEA7, 0xD333A633, 0xD3CB7109, 0xD6D2F14E, 0xD7ABB7B3, 0xDC481F4C, 0xE3E323BA, 0xEF5F306A,
      0xF1D626DC, 0xF476B17D, 0xF7283DA8, 0xFCDB3E0E, 0x4CC91AE4, 0x9BB484AC, 0xA8049B74, 0xB0FBCECD, 0xE118D7F2, 0xED6A2631, 0xFF3FCF4D};

   // Y5R lit materials tone-compress in-shader, per channel, before fog and the cb2[15] scale: F(u) = sqrt(max(0, 1 - exp(-u)))
   // with u = color * cb2[6].x (exposure). Their highlights never pass 1, so the fp16 scene alone gives Y5R no HDR. The curve
   // is continued past u = pivot along its tangent, E(u) = F(min(u, p)) + exp(-p) * max(u - p, 0) inside the sqrt: vanilla
   // bit for bit below p, rising instead of saturating above. The grade rebuilds the vanilla SDR scene from it
   // (YRC_Y5VanillaMaterialCurve; both take the constants from Includes/GameCBuffers.hlsl). Pivot 1.2 keeps vanilla up to
   // 0.84 of white.
   constexpr float y5_material_curve_pivot = YRC_Y5_MATERIAL_CURVE_PIVOT;
   constexpr float y5_material_curve_slope = YRC_Y5_MATERIAL_CURVE_SLOPE;
#if DEVELOPMENT
   std::atomic<uint32_t> y5_material_curve_patches = 0; // Shaders patched so far
#endif

   // All 294 Y5R shaders with the curve (292 lit materials, fx_rigid_snow, the unused ps_tonemap) compile it in place on one
   // temp register rN, with no modifiers: "mul rN, rN, l(-1.442695)", "exp rN, rN", "add rN, -rN, l(1)", then max and sqrt.
   // Y3R/Y4R have no shader with that constant. Adds temp rT and inserts "add rT, rN, -p", "max rT, rT, 0", "min rN, rN, p"
   // before the mul and "mad rN, rT, slope, rN" after the add. Returns the new token stream, empty if the shader has no curve.
   std::vector<uint32_t> PatchY5MaterialToneCurve(const uint32_t* tokens, size_t count)
   {
      constexpr uint32_t neg_log2e = 0xBFB8AA3B; // -1.442695f
      size_t temps_at = count;
      size_t mul_at = count;
      for (size_t i = 0; i < count;)
      {
         const uint32_t opcode = DECODE_D3D10_SB_OPCODE_TYPE(tokens[i]);
         const size_t length = opcode == D3D10_SB_OPCODE_CUSTOMDATA ? (i + 1 < count ? tokens[i + 1] : 0) : DECODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(tokens[i]);
         if (length == 0 || i + length > count)
            return {};
         if (opcode == D3D10_SB_OPCODE_DCL_TEMPS)
            temps_at = i;
         // mul: opcode, dst (token + index), src (token + index), 4-component literal (token + 4 values)
         else if (opcode == D3D10_SB_OPCODE_MUL && length == 10 && std::ranges::any_of(tokens + i + 6, tokens + i + 10, [](uint32_t v)
                                                                      { return v == neg_log2e; }))
         {
            mul_at = i;
            break;
         }
         i += length;
      }
      if (temps_at == count || mul_at == count)
         return {};

      const uint32_t* mul = tokens + mul_at;
      const auto is_plain_temp = [](uint32_t operand)
      { return DECODE_D3D10_SB_OPERAND_TYPE(operand) == D3D10_SB_OPERAND_TYPE_TEMP && !DECODE_IS_D3D10_SB_OPERAND_EXTENDED(operand); };
      const bool literal_is_curve = std::ranges::all_of(mul + 6, mul + 10, [](uint32_t v)
         { return v == neg_log2e || v == 0; });
      if (mul[0] != (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MUL) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10)) || !is_plain_temp(mul[1]) || !is_plain_temp(mul[3]) || mul[2] != mul[4] ||
          DECODE_D3D10_SB_OPERAND_TYPE(mul[5]) != D3D10_SB_OPERAND_TYPE_IMMEDIATE32 || !literal_is_curve)
         return {};
      const size_t exp_at = mul_at + 10;
      const size_t add_at = exp_at + 5;
      if (add_at >= count || DECODE_D3D10_SB_OPCODE_TYPE(tokens[exp_at]) != D3D10_SB_OPCODE_EXP || DECODE_D3D10_SB_OPCODE_TYPE(tokens[add_at]) != D3D10_SB_OPCODE_ADD)
         return {};
      const size_t add_end = add_at + DECODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(tokens[add_at]);
      if (add_end > count || tokens[add_at + 2] != mul[2]) // The add writes rN back
         return {};

      // Operands are the mul's own tokens with another register index or literal: same write mask, swizzle and literal type.
      const uint32_t n = mul[2];
      const uint32_t t = tokens[temps_at + 1];
      const auto op_literal = [&](D3D10_SB_OPCODE_TYPE opcode, uint32_t dst, uint32_t src, float value)
      {
         const uint32_t v = std::bit_cast<uint32_t>(value);
         return std::array<uint32_t, 10>{uint32_t(ENCODE_D3D10_SB_OPCODE_TYPE(opcode) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10)), mul[1], dst, mul[3], src, mul[5], v, v, v, v};
      };
      const auto add = op_literal(D3D10_SB_OPCODE_ADD, t, n, -y5_material_curve_pivot);
      const auto max = op_literal(D3D10_SB_OPCODE_MAX, t, t, 0.f);
      const auto min = op_literal(D3D10_SB_OPCODE_MIN, n, n, y5_material_curve_pivot);
      const uint32_t slope = std::bit_cast<uint32_t>(y5_material_curve_slope);
      const std::array<uint32_t, 12> mad = {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAD) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(12), mul[1], n, mul[3], t, mul[5], slope, slope, slope, slope, mul[3], n};

      std::vector<uint32_t> out;
      out.reserve(count + add.size() * 3 + mad.size());
      out.insert(out.end(), tokens, tokens + mul_at);
      out[temps_at + 1] = t + 1;
      out.insert(out.end(), add.begin(), add.end());
      out.insert(out.end(), max.begin(), max.end());
      out.insert(out.end(), min.begin(), min.end());
      out.insert(out.end(), tokens + mul_at, tokens + add_end);
      out.insert(out.end(), mad.begin(), mad.end());
      out.insert(out.end(), tokens + add_end, tokens + count);
#if DEVELOPMENT
      y5_material_curve_patches++;
#endif
      return out;
   }

   // A Luma shader is usable only once compiled; true when all the named ones are. The caller holds s_mutex_shader_objects.
   template <typename T, typename... Names>
   bool HasShaders(const T& shaders, Names... names)
   {
      const auto has = [&](uint32_t name)
      {
         const auto it = shaders.find(name);
         return it != shaders.end() && it->second;
      };
      return (has(names) && ...);
   }

   // The scene chain's targets: swapchain-sized, or scaled by the game's render scale (Y5R also by its dynamic resolution),
   // which keeps the output aspect ratio up to a pixel of rounding (as Core's SwapchainAspectRatio upgrade filter).
   bool IsSceneSized(const DeviceData& device_data, uint32_t width, uint32_t height)
   {
      return height > 1 && std::abs(float(width) - float(height) * device_data.output_resolution.x / device_data.output_resolution.y) <= 1.f;
   }

   bool GetTextureDesc(ID3D11Resource* resource, D3D11_TEXTURE2D_DESC* desc)
   {
      com_ptr<ID3D11Texture2D> texture;
      if (!resource || FAILED(resource->QueryInterface(&texture)))
         return false;
      texture->GetDesc(desc);
      return true;
   }
} // namespace

#if DEVELOPMENT
namespace
{
   // The "color correct" family (ps_ccr_* x32, ps_color_collection, fx_ccr_*_mask): the pass that carries the HDR tonemap.
   const std::unordered_set<uint32_t> ccr_hashes = {0x00189B34, 0x085CB5DC, 0x0BD7E699, 0x0C663121, 0x0D70385F, 0x105A4974, 0x16366D61, 0x1A8B8E14, 0x1C68CAB4,
      0x299FC101, 0x2B9CB5CF, 0x2F9F68B6, 0x303C672B, 0x3B334605, 0x3F7303BE, 0x601ABB87, 0x62801D10, 0x653BBD8E, 0x65E3DED9, 0x66523C57, 0x70E5BEF5, 0x9B4BFD8A,
      0xAEE6B7E1, 0xB156D307, 0xC011BB2A, 0xC3458723, 0xC3925A82, 0xCF6F5FDB, 0xD05BC597, 0xD2AD713B, 0xD63B6BAF, 0xD66AFCDA, 0xF15A0660, 0xFB2C3B56,
      /*Y5R (the passthrough is tracked where it is flagged)*/ 0x001AD7B6, 0x08331D82, 0x17BEE07A, 0x1A2D4AFC, 0x1A7D81B7, 0x1E44D7D5, 0x20F8A7B4, 0x2D28B9B2, 0x503019AC,
      0x670F7EB8, 0x6685D7B1, 0x6A28984F, 0x75E55CB8, 0x775B9A3B, 0x7D337F2C, 0x8064391A, 0x8ABFD626, 0x8BF213CE, 0x8D537B02, 0x99CE55A0, 0x9EF13462, 0xAE834F02,
      0xAF16C40A, 0xC79BC4D2, 0xCB880511, 0xCB9D04EA, 0xD4804F2C, 0xD872BF99, 0xE0E8F167, 0xE4D6C818, 0xED543AFF, 0xF1C1599F, 0xF3F2937F, 0xF7293E61};
   const std::unordered_set<uint32_t> video_hashes = {0xFD02F404 /*ps_sofdec*/, 0xC9782177 /*ps_sofdec_qloc*/, 0xB09E517F /*ps_sofdec_h264 (Y4R/Y5R)*/};
   // Passes whose HDR behavior depends on their target and blend state, which only runtime shows.
   const std::unordered_map<uint32_t, const char*> watched_hashes = {{0x716ADB18, "focus_blur_pass1 (DoF)"}, {0x04359FA6, "focus_blur_pass2 (DoF)"},
      {0x74E5C6AC, "focus_blur_pass2_mask (DoF)"}, {0xFD02F404, "ps_sofdec"}, {0xC9782177, "ps_sofdec_qloc"}, {0xB09E517F, "ps_sofdec_h264"}, {0xE1631197, "ps_haze"}, {0x3A7B40E4, "ps_afterimage01"},
      {0xFD4620B9, "fx_refraction / fx_blood_floor_refraction"}, {0x7814519F, "fx_track_blur"}, {0xFBA57AE9, "CAS scaled (cs)"}, {0x82DA801B, "CMAA2 apply (cs)"}, {0x4A57A803, "fx_camera_blur"},
      {0x0A4BB34E, "fx_rdiffusion"}, {0x24726E96, "ps_lerp"}, {0xDB9BA88B, "ps_lerp (Y4R)"}, {0x716D6221, "fx_afterimage01 (Y4R)"}, {0xCAEFD55C, "ps_grayscale"}, {0x495BB3CA, "fx_lens_flare"},
      {0x54A5E7AC, "ps_down_sample (glow source) / ps_cubic (Y4R)"}, {0x66633BAD, "ps_down_sample (Y4R glow source)"},
      {0xB8414674, "glow_pass0"}, {0x9083BF34, "glow_pass2"}, {0x9E617E0A, "glow_pass2 (Y4R)"},
      {0x54D6A534, "ps_down_sample_2x4 (Y5R glow source)"}, {0xD31A6374, "glow_pass0 (Y5R)"}, {0x5F37CDE1, "glow_pass2 (Y5R)"}, {0x0B55238C, "with_glare (Y5R exposure)"},
      {0x33E5746C, "fx_afterimage01 (Y5R)"}, {0x20D7E1BA, "ps_haze (Y5R)"}, {0xC389105B, "fx_track_blur (Y5R)"}, {0x0014DD95, "focus_blur_pass2 (Y5R DoF)"},
      {0xF9DF166D, "focus_blur_pass2_tex_a (Y5R DoF)"}, {0xE118D7F2, "focus_blur_pass1_5_blur (Y5R DoF)"}};
   // DoF composites into the scene: focus_blur_pass2 and pass2_mask (Y3R/Y4R, then Y5R).
   const std::unordered_set<uint32_t> dof_composite_pixel_shaders = {0x04359FA6, 0x74E5C6AC, 0x0014DD95, 0xEA4513EB};
   // One-time log lines that have no shader hash of their own.
   enum LoggedEvent : uint32_t
   {
      SMAARanUnpredicated,
      SMAARanPredicated,
      SMAADepthRejected,
      NativeFXAARan,
      ASSAORanNatively,
      ASSAOReplaced,
      GlowDownsampleSampler,
      GlowReexpandOffsets,
      SMAAReplacedFXAA,
      CASReplacedByCopy,
      GlowPass2WeightsDiffer,
   };
   constexpr uint32_t log_interval_frames = 120;
   constexpr uint32_t max_exposure_reads = 16; // per log frame: each one is a staging copy and a GPU sync

   void LogFormatted(reshade::log::level level, const char* format, auto... args)
   {
      char line[512];
      snprintf(line, sizeof(line), format, args...);
      reshade::log::message(level, line);
   }
} // namespace
#endif

struct YakuzaRCDeviceData final : public GameDeviceData
{
   // SMAA inputs: a snapshot of the gamma canvas (the target may be the canvas itself, so SMAA cannot also sample it) and
   // its linear-light decode. SMAA writes its output into the snapshot (only edge detection reads it, before that); RCAS
   // then sharpens it into the linear texture, free by then. Recreated when the canvas size or format changes.
   com_ptr<ID3D11Texture2D> smaa_gamma_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11Texture2D> smaa_linear_texture;
   com_ptr<ID3D11UnorderedAccessView> smaa_linear_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11RenderTargetView> smaa_linear_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;

   // Per frame
   com_ptr<ID3D11ShaderResourceView> depth_srv; // Scene depth for predication, null if no reader ran
   bool cmaa2_replaced = false;                 // The CMAA2 chain of this frame is skipped, its apply runs SMAA
   bool smaa_ran = false;                       // The game's CAS becomes a copy, its scaling CAS a plain resample (RCAS sharpens)
   bool assao_replaced = false;                 // XeGTAO ran at the ASSAO prepare: the chain is skipped, its apply draws XeGTAO
   com_ptr<ID3D11Resource> glow_level0;         // The level the glow downsample wrote, set once the Luma Bloom prefilter ran on it
   bool glow_replaced = false;                  // Luma Bloom ran at glow_pass0: pass1 is skipped, pass2 composites it
   bool scene_done = false;                     // The glow downsample (the first post pass) ran: the UI shaders draw the HUD

   // Luma Bloom: the prefiltered glow source [0], glow_pass0's output from it [1] (DrawBloom's input), and a view with every
   // mip of DrawBloom's result (its SRV shows mip 0 only).
   com_ptr<ID3D11ShaderResourceView> glow_srvs[2];
   com_ptr<ID3D11RenderTargetView> glow_rtvs[2];
   com_ptr<ID3D11Resource> bloom_resource;
   com_ptr<ID3D11ShaderResourceView> bloom_all_mips_srv;

   // XeGTAO scratch, at the depth's size. The size is kept even when the allocation failed: a null set then means
   // "failed", and it is not retried every frame.
   com_ptr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[gtao_depth_mip_count];
   com_ptr<ID3D11ShaderResourceView> gtao_depth_mips_srv;
   com_ptr<ID3D11UnorderedAccessView> gtao_working_uavs[2]; // R8G8_UNORM AO + edges ping-pong
   com_ptr<ID3D11ShaderResourceView> gtao_working_srvs[2];
   com_ptr<ID3D11UnorderedAccessView> gtao_final_uav; // R8_UNORM AO, read by the apply
   com_ptr<ID3D11ShaderResourceView> gtao_final_srv;
   uint32_t gtao_width = 0;
   uint32_t gtao_height = 0;
   com_ptr<ID3D11Buffer> gtao_knobs_cb; // immutable, recreated when a knob changes
   float gtao_knobs[8] = {};

   void ReleaseGlowTextures()
   {
      for (int i = 0; i < 2; i++)
      {
         glow_srvs[i].reset();
         glow_rtvs[i].reset();
      }
   }

   void ReleaseGTAOScratch()
   {
      for (auto& uav : gtao_depth_mip_uavs)
         uav.reset();
      gtao_depth_mips_srv.reset();
      for (auto& uav : gtao_working_uavs)
         uav.reset();
      for (auto& srv : gtao_working_srvs)
         srv.reset();
      gtao_final_uav.reset();
      gtao_final_srv.reset();
      gtao_knobs_cb.reset();
      gtao_width = 0;
      gtao_height = 0;
   }

#if DEVELOPMENT
   // Per frame
   bool drew_ccr = false;
   bool drew_video = false;
   uint32_t ccr_hash = 0;
   std::vector<std::array<float, 4>> exposure_samples; // Material cb2[6].x (Y5R curve exposure) and the output scale .xyz
   uint32_t logged_material_curve_patches = 0;
   uint32_t exposure_reads = 0;
   // Across frames
   bool previous_frame_drew_ccr = true;
   uint32_t frames_without_ccr = 0;
   uint32_t last_logged_ccr_hash = 0;
   std::vector<float> last_logged_ccr_cb5;
   std::vector<float> last_logged_assao_cb0;
   std::vector<float> last_logged_glow_cbs[2]; // glow_pass0, glow_pass2
   std::vector<float> last_logged_dof_cb5;
   uint32_t glow_pass1_draws = 0;                           // this frame
   std::vector<float> last_logged_glow_pass1[8];            // PS cb5[0..3], VS cb7[0], target and source size per draw
   com_ptr<ID3D11Resource> glow_source;                     // The glow downsample's scene-sized t0, from a previous frame
   std::unordered_set<uint32_t> logged_glow_source_writers; // Pixel shaders seen rendering into it
   std::unordered_set<uint32_t> seen_blend_hashes;          // Pixel shaders already checked by the blended-effect trap
   std::unordered_set<uint32_t> logged_watched_hashes;
   uint32_t logged_events = 0; // LoggedEvent bits
   std::unordered_set<uint32_t> checked_custom_size_hashes;
   // Staging copy for the one-shot predication mask readback (see LogPredicationStats), allocated on first use.
   com_ptr<ID3D11Texture2D> pred_measure_staging;
   bool pred_measure_pending = false;

   // True the first time only.
   bool FirstLog(LoggedEvent event)
   {
      const uint32_t bit = 1u << event;
      const bool first = (logged_events & bit) == 0;
      logged_events |= bit;
      return first;
   }
#endif
};

class GameYakuzaRC final : public Game
{
   static YakuzaRCDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<YakuzaRCDeviceData*>(device_data.game);
   }

   // SMAA (+ RCAS) runs on the post-ccr fp16 canvas at its scene size (before the render scale resample); anything else (SDR
   // targets) keeps the game's own AA. The caller holds s_mutex_shader_objects.
   static bool CanRunSMAA(const DeviceData& device_data, const D3D11_TEXTURE2D_DESC& color_desc)
   {
      return g_smaa_enable && color_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && color_desc.SampleDesc.Count == 1 && color_desc.ArraySize == 1 && IsSceneSized(device_data, color_desc.Width, color_desc.Height) && HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h, "YRC Sharpen PS"_h) && HasShaders(device_data.native_compute_shaders, "YRC SMAA Linearize CS"_h);
   }

#if DEVELOPMENT
   // One-shot readback of the predication mask (port of the BL GOTY calibration aid), so the tolerance is calibrated from
   // numbers rather than screenshots. On a well-tuned frame the mask is 0 nearly everywhere, so this reports COVERAGE at
   // the level SMAA compares against (0.5) plus the shape either side of it, over every texel. Copies on the frame the
   // button is pressed and maps on a later one (non-blocking). Immediate context only.
   static void LogPredicationStats(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, YakuzaRCDeviceData* game_device_data, ID3D11Texture2D* mask)
   {
      if (!g_smaa_pred_measure && !game_device_data->pred_measure_pending)
         return;

      D3D11_TEXTURE2D_DESC mask_desc;
      mask->GetDesc(&mask_desc);
      D3D11_TEXTURE2D_DESC staging_desc = {};
      if (game_device_data->pred_measure_staging)
         game_device_data->pred_measure_staging->GetDesc(&staging_desc);
      if (staging_desc.Width != mask_desc.Width || staging_desc.Height != mask_desc.Height)
      {
         staging_desc = mask_desc;
         staging_desc.Usage = D3D11_USAGE_STAGING;
         staging_desc.BindFlags = 0;
         staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         staging_desc.MiscFlags = 0;
         game_device_data->pred_measure_staging.reset();
         game_device_data->pred_measure_pending = false;
         if (FAILED(native_device->CreateTexture2D(&staging_desc, nullptr, &game_device_data->pred_measure_staging)))
            return;
      }

      if (!game_device_data->pred_measure_pending)
      {
         g_smaa_pred_measure = false;
         native_device_context->CopyResource(game_device_data->pred_measure_staging.get(), mask);
         game_device_data->pred_measure_pending = true;
         return;
      }

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(game_device_data->pred_measure_staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return; // Still in flight, retry next frame

      constexpr uint32_t bins = 256;
      uint64_t histogram[bins] = {};
      uint64_t total = 0;
      uint64_t non_finite = 0;
      for (UINT y = 0; y < mask_desc.Height; y++)
      {
         const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch);
         for (UINT x = 0; x < mask_desc.Width; x++)
         {
            const float v = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
            total++;
            // A range test, not a clamp: NaN fails both comparisons and would index outside the array once cast.
            if (!(v >= 0.f && v <= 1.f))
               non_finite++;
            else
               histogram[uint32_t(v * float(bins - 1))]++;
         }
      }
      native_device_context->Unmap(game_device_data->pred_measure_staging.get(), 0);
      game_device_data->pred_measure_pending = false;

      const auto fraction_above = [&](float level)
      {
         uint64_t hits = 0;
         for (uint32_t b = uint32_t(level * float(bins - 1)) + 1u; b < bins; b++)
            hits += histogram[b];
         return 100.0 * double(hits) / double(total);
      };
      const auto percentile = [&](double p)
      {
         const uint64_t target = uint64_t(p * double(total));
         uint64_t running = 0;
         for (uint32_t b = 0; b < bins; b++)
         {
            running += histogram[b];
            if (running >= target)
               return float(b) / float(bins - 1);
         }
         return 1.f;
      };
      LogFormatted(reshade::log::level::info, "[YRC-Pred] tol=%.4f | FIRES(>0.5)=%.3f%% | >0.1=%.3f%% >0.25=%.3f%% >0.75=%.3f%% >0.9=%.3f%% | flat(bin0)=%.2f%% | p50=%.3f p90=%.3f p99=%.3f p999=%.3f | nonfinite=%llu | %ux%u",
         g_smaa_pred_tolerance, fraction_above(0.5f), fraction_above(0.1f), fraction_above(0.25f), fraction_above(0.75f), fraction_above(0.9f), 100.0 * double(histogram[0]) / double(total),
         percentile(0.5), percentile(0.9), percentile(0.99), percentile(0.999), (unsigned long long)non_finite, mask_desc.Width, mask_desc.Height);
   }
#endif

   // SMAA from "color" into "target" (the same resource for CMAA2, separate for FXAA), then RCAS when sharpness > 0.
   // Returns false, leaving everything untouched, if SMAA can't run.
   static bool RunSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool* updated_cbuffers, ID3D11Resource* color, ID3D11Resource* target)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      D3D11_TEXTURE2D_DESC color_desc;
      if (!GetTextureDesc(color, &color_desc) || !AreResourcesEqual(color, target))
         return false;

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at".
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!CanRunSMAA(device_data, color_desc))
         return false;

      D3D11_TEXTURE2D_DESC snapshot_desc = {};
      if (game_device_data.smaa_gamma_texture)
         game_device_data.smaa_gamma_texture->GetDesc(&snapshot_desc);
      if (snapshot_desc.Width != color_desc.Width || snapshot_desc.Height != color_desc.Height || snapshot_desc.Format != color_desc.Format)
      {
         game_device_data.smaa_gamma_texture.reset();
         game_device_data.smaa_gamma_srv.reset();
         game_device_data.smaa_gamma_rtv.reset();
         game_device_data.smaa_linear_texture.reset();
         game_device_data.smaa_linear_uav.reset();
         game_device_data.smaa_linear_srv.reset();
         game_device_data.smaa_linear_rtv.reset();
         game_device_data.smaa_predication_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         D3D11_TEXTURE2D_DESC desc = color_desc;
         desc.MipLevels = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_gamma_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) || FAILED(native_device->CreateRenderTargetView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return false;
         }
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_linear_texture)) || FAILED(native_device->CreateUnorderedAccessView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_uav)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv)) || FAILED(native_device->CreateRenderTargetView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_rtv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return false;
         }
         // Without it SMAA simply runs unpredicated.
         desc.Format = DXGI_FORMAT_R16_FLOAT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> predication_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &predication_texture)) || FAILED(native_device->CreateUnorderedAccessView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_uav)) || FAILED(native_device->CreateShaderResourceView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_srv)))
         {
            game_device_data.smaa_predication_uav.reset();
            game_device_data.smaa_predication_srv.reset();
         }
      }

      uint4 depth_size;
      DXGI_FORMAT depth_format;
      GetResourceInfo(game_device_data.depth_srv.get(), depth_size, depth_format);
      bool predicate = game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "YRC SMAA Predication CS"_h) && depth_size.x == color_desc.Width && depth_size.y == color_desc.Height;
#if DEVELOPMENT
      predicate = predicate && g_smaa_predication;
#endif

      native_device_context->CopyResource(game_device_data.smaa_gamma_texture.get(), color);
      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11UnorderedAccessView* const linear_uav = game_device_data.smaa_linear_uav.get();
         ID3D11ShaderResourceView* const gamma_srv = game_device_data.smaa_gamma_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &linear_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &gamma_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at("YRC SMAA Linearize CS"_h).get(), nullptr, 0);
         native_device_context->Dispatch((color_desc.Width + 7) / 8, (color_desc.Height + 7) / 8, 1);
         if (predicate)
         {
            // The game can still have the depth bound as a writable DSV here, and D3D11 then silently nulls the depth SRV
            // (the mask came out all zero): unbind the output merger for the dispatch.
            com_ptr<ID3D11RenderTargetView> om_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> om_dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &om_rtvs[0], &om_dsv);
            if (om_dsv)
               native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const depth_srv = game_device_data.depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &depth_srv);
#if DEVELOPMENT
            // The runtime can still reject the depth SRV (another conflicting binding).
            com_ptr<ID3D11ShaderResourceView> bound_depth_srv;
            native_device_context->CSGetShaderResources(0, 1, &bound_depth_srv);
            if (!bound_depth_srv && game_device_data.FirstLog(SMAADepthRejected))
               LogFormatted(reshade::log::level::warning, "[YRC] frame %u SMAA predication: depth SRV rejected by the runtime", cb_luma_global_settings.FrameIndex);
#endif
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::compute, LumaConstantBufferType::LumaData, g_game_profile.reversed_depth ? 1u : 0u, 0, g_smaa_pred_tolerance);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at("YRC SMAA Predication CS"_h).get(), nullptr, 0);
            native_device_context->Dispatch((color_desc.Width + 7) / 8, (color_desc.Height + 7) / 8, 1);

            if (om_dsv)
            {
               ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
               for (size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                  rtvs[i] = om_rtvs[i].get();
               native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, om_dsv.get());
            }
         }
         compute_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      if (predicate)
      {
         com_ptr<ID3D11Resource> mask_resource;
         game_device_data.smaa_predication_srv->GetResource(&mask_resource);
         com_ptr<ID3D11Texture2D> mask;
         if (SUCCEEDED(mask_resource->QueryInterface(&mask)))
            LogPredicationStats(native_device, native_device_context, &game_device_data, mask.get());

         // Calibration view: the single-channel mask lands in red, replacing the frame (black on flat surfaces, red across
         // silhouettes; all red = tolerance too low, all black = too high).
         if (g_smaa_pred_debug && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
         {
            DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
            debug_state.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), game_device_data.smaa_predication_srv.get(), game_device_data.smaa_gamma_rtv.get(), color_desc.Width, color_desc.Height, false);
            debug_state.Restore(native_device_context);
            native_device_context->CopyResource(target, game_device_data.smaa_gamma_texture.get());
            game_device_data.smaa_ran = true;
            return true;
         }
      }
#endif

      // The SMAA shaders read the canvas size from the Luma settings and the predication scale from the Luma data, in both stages.
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, predicate ? 2.f : 1.f);
      *updated_cbuffers = true;
      DrawSMAA(native_device, native_device_context, device_data, game_device_data.smaa_gamma_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), predicate ? game_device_data.smaa_predication_srv.get() : nullptr);

      ID3D11Texture2D* result = game_device_data.smaa_gamma_texture.get();
      if (g_rcas_sharpness > 0.f)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, g_rcas_sharpness);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("YRC Sharpen PS"_h).get(), game_device_data.smaa_gamma_srv.get(), game_device_data.smaa_linear_rtv.get(), color_desc.Width, color_desc.Height, false);
         sharpen_state.Restore(native_device_context);
         result = game_device_data.smaa_linear_texture.get();
      }
      native_device_context->CopyResource(target, result);
      game_device_data.smaa_ran = true;
#if DEVELOPMENT
      if (game_device_data.FirstLog(predicate ? SMAARanPredicated : SMAARanUnpredicated))
         LogFormatted(reshade::log::level::info, "[YRC] frame %u SMAA ran (predication %d, %ux%u)", cb_luma_global_settings.FrameIndex, predicate, color_desc.Width, color_desc.Height);
#endif
      return true;
   }

   // XeGTAO in place of the ASSAO prepare (where the depth is bound for reading, never as the DSV that would null its SRV):
   // prefilter, main pass and two denoisers on the prepare's own depth (t0) and ASSAO constants (b0, for this frame's FOV).
   // The apply then draws the result. Returns false, and the native chain runs, when a shader, an input or the scratch is
   // missing.
   bool RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11ShaderResourceView* depth_srv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // Held through the passes so a shader reload cannot release them mid-use.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const auto& shaders = device_data.native_compute_shaders;
      if (!HasShaders(shaders, "YRC XeGTAO Prefilter Depths CS"_h, "YRC XeGTAO Main Pass CS"_h, "YRC XeGTAO Denoise Pass 1 CS"_h, "YRC XeGTAO Denoise Pass 2 CS"_h) || !HasShaders(device_data.native_pixel_shaders, "YRC GTAO Apply PS"_h))
         return false;

      com_ptr<ID3D11Buffer> assao_cb;
      native_device_context->PSGetConstantBuffers(0, 1, &assao_cb);
      uint4 depth_size;
      DXGI_FORMAT depth_format;
      GetResourceInfo(depth_srv, depth_size, depth_format);
      const uint32_t width = depth_size.x;
      const uint32_t height = depth_size.y;
      if (!assao_cb || width == 0 || height == 0)
         return false;

      if (game_device_data.gtao_width != width || game_device_data.gtao_height != height)
      {
         game_device_data.ReleaseGTAOScratch();
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = width;
         desc.Height = height;
         desc.MipLevels = gtao_depth_mip_count;
         desc.ArraySize = 1;
         desc.Format = DXGI_FORMAT_R32_FLOAT;
         desc.SampleDesc.Count = 1;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> depth_mips_texture; // R32F view-space depth pyramid
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &depth_mips_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(depth_mips_texture.get(), nullptr, &game_device_data.gtao_depth_mips_srv));
         D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
         uav_desc.Format = desc.Format;
         uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
         for (UINT mip = 0; ok && mip < gtao_depth_mip_count; mip++)
         {
            uav_desc.Texture2D.MipSlice = mip;
            ok = SUCCEEDED(native_device->CreateUnorderedAccessView(depth_mips_texture.get(), &uav_desc, &game_device_data.gtao_depth_mip_uavs[mip]));
         }
         desc.MipLevels = 1;
         desc.Format = DXGI_FORMAT_R8G8_UNORM;
         for (int i = 0; ok && i < 2; i++)
         {
            com_ptr<ID3D11Texture2D> working_texture;
            ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &working_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(working_texture.get(), nullptr, &game_device_data.gtao_working_uavs[i])) && SUCCEEDED(native_device->CreateShaderResourceView(working_texture.get(), nullptr, &game_device_data.gtao_working_srvs[i]));
         }
         desc.Format = DXGI_FORMAT_R8_UNORM;
         com_ptr<ID3D11Texture2D> final_texture;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &final_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(final_texture.get(), nullptr, &game_device_data.gtao_final_uav)) && SUCCEEDED(native_device->CreateShaderResourceView(final_texture.get(), nullptr, &game_device_data.gtao_final_srv));
         if (!ok)
            game_device_data.ReleaseGTAOScratch();
         game_device_data.gtao_width = width;
         game_device_data.gtao_height = height;
      }
      if (!game_device_data.gtao_final_srv)
         return false;

#if DEVELOPMENT
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      const float knobs[8] = {g_gtao_final_value_power, g_gtao_radius_override, debug_view, 0.f, 1.f / float(width), 1.f / float(height), 0.f, 0.f};
      if (!game_device_data.gtao_knobs_cb || std::memcmp(game_device_data.gtao_knobs, knobs, sizeof(knobs)) != 0)
      {
         game_device_data.gtao_knobs_cb.reset();
         D3D11_BUFFER_DESC cb_desc = {};
         cb_desc.ByteWidth = sizeof(knobs);
         cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
         cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         const D3D11_SUBRESOURCE_DATA cb_data = {knobs};
         if (FAILED(native_device->CreateBuffer(&cb_desc, &cb_data, &game_device_data.gtao_knobs_cb)))
            return false;
         std::memcpy(game_device_data.gtao_knobs, knobs, sizeof(knobs));
      }

      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      ID3D11Buffer* const cbs[] = {assao_cb.get(), game_device_data.gtao_knobs_cb.get()};
      native_device_context->CSSetConstantBuffers(0, 1, &cbs[0]);
      native_device_context->CSSetConstantBuffers(gtao_knobs_cb_slot, 1, &cbs[1]);
      ID3D11SamplerState* const point_sampler = device_data.sampler_state_point.get();
      native_device_context->CSSetSamplers(0, 1, &point_sampler);

      // Each pass binds its destination UAV before its source SRV: D3D11 otherwise nulls an SRV that still aliases the
      // previous pass's bound UAV.
      ID3D11ShaderResourceView* const null_srv = nullptr;
      ID3D11UnorderedAccessView* const null_uavs[gtao_depth_mip_count] = {};
      const auto pass = [&](uint32_t shader_name_hash, UINT uav_count, ID3D11UnorderedAccessView* const* uavs, ID3D11ShaderResourceView* srv, UINT groups_x, UINT groups_y)
      {
         native_device_context->CSSetShaderResources(0, 1, &null_srv);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, uavs, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &srv);
         native_device_context->CSSetShader(shaders.at(shader_name_hash).get(), nullptr, 0);
         native_device_context->Dispatch(groups_x, groups_y, 1);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, null_uavs, nullptr);
      };
      ID3D11UnorderedAccessView* mip_uavs[gtao_depth_mip_count];
      for (UINT mip = 0; mip < gtao_depth_mip_count; mip++)
         mip_uavs[mip] = game_device_data.gtao_depth_mip_uavs[mip].get();
      ID3D11UnorderedAccessView* const working_uavs[2] = {game_device_data.gtao_working_uavs[0].get(), game_device_data.gtao_working_uavs[1].get()};
      ID3D11UnorderedAccessView* const final_uav = game_device_data.gtao_final_uav.get();
      pass("YRC XeGTAO Prefilter Depths CS"_h, gtao_depth_mip_count, mip_uavs, depth_srv, (width + 15) / 16, (height + 15) / 16);
      pass("YRC XeGTAO Main Pass CS"_h, 1, &working_uavs[0], game_device_data.gtao_depth_mips_srv.get(), (width + 7) / 8, (height + 7) / 8);
      pass("YRC XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], game_device_data.gtao_working_srvs[0].get(), (width + 15) / 16, (height + 7) / 8);
      pass("YRC XeGTAO Denoise Pass 2 CS"_h, 1, &final_uav, game_device_data.gtao_working_srvs[1].get(), (width + 15) / 16, (height + 7) / 8);
      compute_state.Restore(native_device_context);
      return true;
   }

   // At the glow downsample: the 4K glow source (its t0) into the 1024x512 prefilter, clamped per texel as the 8-bit source
   // was, averaging 5x6 taps per texel (Y4R: RMS over 3x the footprint, standing in for its two RMS stages).
   // On success, records the level it writes, which glow_pass0 must read.
   static void PrefilterGlowSource(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.glow_level0.reset();
      com_ptr<ID3D11ShaderResourceView> source_srv;
      native_device_context->PSGetShaderResources(0, 1, &source_srv);
      com_ptr<ID3D11RenderTargetView> level0_rtv;
      native_device_context->OMGetRenderTargets(1, &level0_rtv, nullptr);
      if (!source_srv || !level0_rtv)
         return;

      if (!game_device_data.glow_rtvs[1])
      {
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = glow_prefilter_width;
         desc.Height = glow_prefilter_height;
         desc.MipLevels = 1;
         desc.ArraySize = 1;
         desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
         desc.SampleDesc.Count = 1;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         // [1]'s view is made last: it marks the pair as complete.
         for (int i = 0; i < 2; i++)
         {
            com_ptr<ID3D11Texture2D> texture;
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &texture)) || FAILED(native_device->CreateShaderResourceView(texture.get(), nullptr, &game_device_data.glow_srvs[i])) || FAILED(native_device->CreateRenderTargetView(texture.get(), nullptr, &game_device_data.glow_rtvs[i])))
            {
               game_device_data.ReleaseGlowTextures();
               return;
            }
         }
      }

      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) || !HasShaders(device_data.native_pixel_shaders, "YRC Glow Prefilter PS"_h))
         return;
      DrawStateStack<DrawStateStackType::FullGraphics> prefilter_state;
      prefilter_state.Cache(native_device_context, device_data.uav_max_count);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, g_game_profile.material_tone_curve ? 1u : 0u, g_game_profile.glow_downsample_rms ? 1u : 0u);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), device_data.sampler_state_linear.get(), device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("YRC Glow Prefilter PS"_h).get(), source_srv.get(), game_device_data.glow_rtvs[0].get(), glow_prefilter_width, glow_prefilter_height, false);
      prefilter_state.Restore(native_device_context);
      level0_rtv->GetResource(&game_device_data.glow_level0);
   }

   // Replays the game's own draw (its VS, viewport, blend and other bindings) with a Luma pixel shader reading `srv` at t0
   // (and `sampler` at s0, if given), then restores the game's shader and bindings. Draws nothing when the shader is missing
   // (a reload since the pass that set this draw up).
   static void DrawWithLumaPixelShader(ID3D11DeviceContext* native_device_context, const DeviceData& device_data, const std::function<void()>& draw, uint32_t ps_hash, ID3D11ShaderResourceView* srv, ID3D11SamplerState* sampler = nullptr)
   {
      com_ptr<ID3D11PixelShader> ps;
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         if (const auto it = device_data.native_pixel_shaders.find(ps_hash); it != device_data.native_pixel_shaders.end())
            ps = it->second;
      }
      if (!ps)
         return;
      com_ptr<ID3D11PixelShader> game_ps;
      native_device_context->PSGetShader(&game_ps, nullptr, nullptr);
      com_ptr<ID3D11ShaderResourceView> game_srv;
      native_device_context->PSGetShaderResources(0, 1, &game_srv);
      com_ptr<ID3D11SamplerState> game_sampler;
      if (sampler)
         native_device_context->PSGetSamplers(0, 1, &game_sampler);
      native_device_context->PSSetShader(ps.get(), nullptr, 0);
      native_device_context->PSSetShaderResources(0, 1, &srv);
      if (sampler)
         native_device_context->PSSetSamplers(0, 1, &sampler);
      draw();
      ID3D11ShaderResourceView* const restored_srv = game_srv.get();
      native_device_context->PSSetShaderResources(0, 1, &restored_srv);
      if (sampler)
      {
         ID3D11SamplerState* const restored_sampler = game_sampler.get();
         native_device_context->PSSetSamplers(0, 1, &restored_sampler);
      }
      native_device_context->PSSetShader(game_ps.get(), nullptr, 0);
   }

   // At glow_pass0, in place of it: pass0's gains and scene term on the prefilter (its constants, sampler and scene still
   // bound), then the shared pyramid. Only when pass0 reads the level the glow downsample just wrote (Y5R downsamples twice;
   // its first level may be something else), else the native pyramid runs.
   static bool RunLumaBloom(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      com_ptr<ID3D11ShaderResourceView> level0_srv;
      native_device_context->PSGetShaderResources(1, 1, &level0_srv);
      com_ptr<ID3D11Resource> level0;
      if (level0_srv)
         level0_srv->GetResource(&level0);
      if (!level0 || level0 != game_device_data.glow_level0)
         return false;

      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "Bloom VS"_h, "Copy VS"_h) || !HasShaders(device_data.native_pixel_shaders, "Bloom Prefilter PS"_h, "Bloom Downsample PS"_h, "Bloom Upsample PS"_h, "YRC Glow Gain PS"_h, "YRC Bloom Composite PS"_h))
         return false;
      com_ptr<ID3D11ShaderResourceView> scene_srv;
      native_device_context->PSGetShaderResources(0, 1, &scene_srv);
      if (!scene_srv)
         return false;

      DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
      bloom_state.Cache(native_device_context, device_data.uav_max_count);
      ID3D11ShaderResourceView* const prefilter_srv = game_device_data.glow_srvs[0].get();
      native_device_context->PSSetShaderResources(1, 1, &prefilter_srv);
      ID3D11SamplerState* const linear_sampler = device_data.sampler_state_linear.get();
      native_device_context->PSSetSamplers(2, 1, &linear_sampler); // Luma_YRC_GlowGain.hlsl: the level-0 blur for its clamp
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, g_game_profile.material_tone_curve ? 1u : 0u, 0, g_game_profile.glow_level0_sigma);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("YRC Glow Gain PS"_h).get(), scene_srv.get(), game_device_data.glow_rtvs[1].get(), glow_prefilter_width, glow_prefilter_height, false);
      com_ptr<ID3D11ShaderResourceView> bloom_srv;
      const float sigmas[bloom_mips] = {g_game_profile.glow_level0_sigma, bloom_level_sigma, bloom_level_sigma, bloom_level_sigma, bloom_level_sigma};
      DrawBloom(native_device, native_device_context, device_data, game_device_data.glow_srvs[1].get(), bloom_mips, sigmas, &bloom_srv);
      bloom_state.Restore(native_device_context);
      if (!bloom_srv)
         return false;

      com_ptr<ID3D11Resource> bloom_resource;
      bloom_srv->GetResource(&bloom_resource);
      if (bloom_resource != game_device_data.bloom_resource)
      {
         game_device_data.bloom_all_mips_srv.reset();
         game_device_data.bloom_resource = bloom_resource;
         if (FAILED(native_device->CreateShaderResourceView(bloom_resource.get(), nullptr, &game_device_data.bloom_all_mips_srv)))
            game_device_data.bloom_resource.reset();
      }
      return game_device_data.bloom_all_mips_srv != nullptr;
   }

public:
   // Listed effects get "mov_sat o0.xyzw, o0.xyzw" before the final ret: the clamp their vanilla UNORM target applied.
   // Every other Y5R pixel shader goes through "PatchY5MaterialToneCurve".
   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      constexpr uint32_t ret_token = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_RET) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(1);
      if (type != reshade::api::pipeline_subobject_type::pixel_shader || size % sizeof(uint32_t) != 0 || size < sizeof(uint32_t))
         return nullptr;
      if (!unorm_clamped_effect_pixel_shaders.contains(uint32_t(shader_hash)))
      {
         if (!g_game_profile.material_tone_curve)
            return nullptr;
         const std::vector<uint32_t> patched = PatchY5MaterialToneCurve(reinterpret_cast<const uint32_t*>(code), size / sizeof(uint32_t));
         if (patched.empty())
            return nullptr;
         size = patched.size() * sizeof(uint32_t);
         auto new_code = std::make_unique<std::byte[]>(size);
         std::memcpy(new_code.get(), patched.data(), size);
         return new_code;
      }
      if (reinterpret_cast<const uint32_t*>(code)[size / sizeof(uint32_t) - 1] != ret_token)
         return nullptr;
      // Encoded by hand rather than with ShaderPatching::GetSatInstruction, whose source operand uses the mask selection mode;
      // fxc encodes sources as swizzles (checked on a patched container with fxc /dumpbin).
      constexpr uint32_t operand_o0 = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_OUTPUT) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      constexpr uint32_t patch[] = {
         ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true),
         operand_o0 | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL, 0,
         operand_o0 | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE, 0};
      static_assert(patch[0] == 0x05002036 && patch[1] == 0x001020F2 && patch[3] == 0x00102E46);
      constexpr size_t patch_size = sizeof(patch);
      const size_t ret_offset = size - sizeof(uint32_t);
      auto new_code = std::make_unique<std::byte[]>(size + patch_size);
      std::memcpy(new_code.get(), code, ret_offset);
      std::memcpy(new_code.get() + ret_offset, patch, patch_size);
      std::memcpy(new_code.get() + ret_offset + patch_size, code + ret_offset, sizeof(uint32_t));
      size += patch_size;
      return new_code;
   }

   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", 1},
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Gamma-space post processing (the vanilla chain and HUD are 2.2 gamma, UNORM, no sRGB views), 1.0 = paper white.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The HUD is drawn straight onto the post-AA scene: the grade pre-scales the scene by game/UI paper white.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // No game shader binds b8-b10 (b12/b13 are bound by the engine on every draw).
      luma_settings_cbuffer_index = 10;
      luma_data_cbuffer_index = 9;

      default_luma_global_game_settings.VideoAutoHDREnable = cb_luma_global_settings.GameSettings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = cb_luma_global_settings.GameSettings.VideoAutoHDRBoost = 0.5f; // peak ~165 nits
      default_luma_global_game_settings.Dithering = cb_luma_global_settings.GameSettings.Dithering = 1.f;
      default_luma_global_game_settings.BloomIntensity = cb_luma_global_settings.GameSettings.BloomIntensity = 1.f;

      native_shaders_definitions.emplace(CompileTimeStringHash("YRC SMAA Linearize CS"), ShaderDefinition{"Luma_YRC_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC SMAA Predication CS"), ShaderDefinition{"Luma_YRC_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC Sharpen PS"), ShaderDefinition{"Luma_YRC_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
      // XeGTAO passes (Luma_YRC_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_YRC_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC XeGTAO Main Pass CS"), ShaderDefinition{"Luma_YRC_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_YRC_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_YRC_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC GTAO Apply PS"), ShaderDefinition{"Luma_YRC_GTAOApply", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC Glow Prefilter PS"), ShaderDefinition{"Luma_YRC_Bloom", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "glow_prefilter_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC Glow Gain PS"), ShaderDefinition{"Luma_YRC_GlowGain", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "glow_gain_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YRC Bloom Composite PS"), ShaderDefinition{"Luma_YRC_Bloom", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "bloom_composite_ps"});
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, NAME, "BloomEnable", g_luma_bloom_enable);
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;

      ImGui::SeparatorText("Anti-Aliasing");

      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's CMAA2/FXAA with SMAA and its CAS with RCAS (requires Anti-Aliasing enabled in the game's graphics settings).");

      ImGui::BeginDisabled(!g_smaa_enable);
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f);
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Sharpening applied on top of SMAA (0 = off).");
      DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness");
#if DEVELOPMENT
      // Predication only relaxes the edge threshold back to base ULTRA on geometry, never below, so off is strictly worse:
      // a bisect switch and calibration tools for devs (session-only), shipped on at 0.02.
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.");
      ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
      ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
      if (ImGui::Button("Measure Predication"))
         g_smaa_pred_measure = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Read the mask back and log its distribution to ReShade.log.\nStand still, set a tolerance, press; repeat per value and compare the lines.\nFIRES(>0.5) is the share of the frame that regains base sensitivity.");
#endif
      ImGui::EndDisabled();

      ImGui::SeparatorText("Bloom");

      if (ImGui::Checkbox("Luma Bloom Enable", &g_luma_bloom_enable))
         reshade::set_config_value(nullptr, NAME, "BloomEnable", g_luma_bloom_enable);
      if (ImGui::IsItemHovered())
         // Off the canon "wider, softer HDR bloom": its width is fitted to the vanilla glow and it stays within 1 per channel.
         ImGui::SetTooltip("Replaces the game's bloom with a smoother version of the same glow.");

      ImGui::BeginDisabled(!g_luma_bloom_enable);
      if (ImGui::SliderFloat("Bloom Intensity", &gs.BloomIntensity, 0.f, 2.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Bloom strength (1 = vanilla, 0 = none).");
      if (DrawResetButton(gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, "BloomIntensity"))
         device_data.cb_luma_global_settings_dirty = true;
      ImGui::EndDisabled();

      ImGui::SeparatorText("Ambient Occlusion");

      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
         reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires SSAO enabled in the game's graphics settings).");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO).");
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f, "%.3f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("0 = the game's own SSAO radius (0.8); > 0 overrides it, in the same view units.");
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws diagnostics through the game's SSAO apply (multiplied onto the scene). Depth gradient flat or blocky = wrong input;\nNormals: camera-facing surfaces bright; AO x8 = spot broad over-occlusion.");
#endif
      ImGui::EndDisabled();
#endif
#if DEVELOPMENT
      ImGui::Checkbox("Skip AO Apply", &g_gtao_skip_apply);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Calibration reference: the frame without any AO, whether XeGTAO or the game's SSAO is active.");
#endif

      ImGui::SeparatorText("Effects");

      bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
      if (ImGui::Checkbox("Video AutoHDR", &video_auto_hdr))
      {
         gs.VideoAutoHDREnable = video_auto_hdr ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(!video_auto_hdr);
      if (ImGui::SliderFloat("Video HDR Boost", &gs.VideoAutoHDRBoost, 0.f, 1.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Video highlight strength (0 = off).");
      if (DrawResetButton(gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost"))
         device_data.cb_luma_global_settings_dirty = true;
      ImGui::EndDisabled();

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new YakuzaRCDeviceData;
      reshade::log::message(reshade::log::level::info, (std::string("[YRC] game profile: ") + g_game_profile.name).c_str());
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<YakuzaRCDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool is_compute = (stages & reshade::api::shader_stage::compute) != 0;
      const uint32_t hash = is_compute ? original_shader_hashes.compute_shaders[0] : original_shader_hashes.pixel_shaders[0];

      // Y4R/Y5R also record draws (materials, depth prepasses) on deferred contexts from other threads. Every hook below
      // keeps unsynchronized per-device state and relies on the immediate context's order (a deferred draw reaches the GPU
      // only at ExecuteCommandList), and the DEV readbacks need the immediate context too.
      if (!cmd_list_data.is_primary)
         return DrawOrDispatchOverrideType::None;
#if DEVELOPMENT
      const bool log_frame = cb_luma_global_settings.FrameIndex % log_interval_frames == 0;
#endif

      // Before the scene is done the same shaders draw in-world geometry (p_dvtx) or off-screen targets: left alone.
      if (g_hide_ui && !is_compute && game_device_data.scene_done && ui_pixel_shaders.contains(hash))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         uint4 size = {};
         DXGI_FORMAT format;
         if (rtv)
            GetResourceInfo(rtv.get(), size, format);
         if (IsSceneSized(device_data, size.x, size.y))
            return DrawOrDispatchOverrideType::Replaced;
      }

      if (!is_compute && depth_reader_pixel_shaders.contains(hash))
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
         if (srv)
            srv->GetDesc(&srv_desc);
         const bool is_depth = srv && srv_desc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS && srv_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D;
         if (is_depth)
            game_device_data.depth_srv = srv;
         // The ASSAO prepare decides for the whole chain: once skipped, the apply must draw XeGTAO (its inputs were never built).
         if (hash == assao_prepare_pixel_shader)
         {
            game_device_data.assao_replaced = g_gtao_enable && is_depth && RunXeGTAO(native_device, native_device_context, device_data, srv.get());
#if DEVELOPMENT
            if (game_device_data.FirstLog(game_device_data.assao_replaced ? ASSAOReplaced : ASSAORanNatively))
               LogFormatted(reshade::log::level::info, "[YRC] frame %u ASSAO %s (XeGTAO enabled %d, depth %d)", cb_luma_global_settings.FrameIndex, game_device_data.assao_replaced ? "replaced by XeGTAO" : "ran natively", g_gtao_enable, is_depth);
#endif
            if (game_device_data.assao_replaced)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      else if (!is_compute && game_device_data.assao_replaced && assao_pre_apply_pixel_shaders.contains(hash))
      {
         return DrawOrDispatchOverrideType::Replaced;
      }
#if DEVELOPMENT
      // The "no AO" calibration reference, native or XeGTAO: the apply is a plain multiply, so skipping it removes all AO.
      else if (!is_compute && hash == assao_apply_pixel_shader && g_gtao_skip_apply)
      {
         return DrawOrDispatchOverrideType::Replaced;
      }
#endif
      // The game's apply draw (full-screen VS, viewport, multiply blend onto the scene RT) with the XeGTAO reader as its PS.
      else if (!is_compute && hash == assao_apply_pixel_shader && game_device_data.assao_replaced)
      {
         // Without the shader the frame goes without AO: the native inputs were never built.
         if (original_draw_dispatch_func)
            DrawWithLumaPixelShader(native_device_context, device_data, *original_draw_dispatch_func, "YRC GTAO Apply PS"_h, game_device_data.gtao_final_srv.get());
         return DrawOrDispatchOverrideType::Replaced;
      }
      else if (is_compute && cmaa2_pre_apply_compute_shaders.contains(hash))
      {
         // The first pass decides for the whole chain: once skipped, the apply must run SMAA (its inputs were never built).
         if (hash == cmaa2_first_compute_shader)
         {
            com_ptr<ID3D11ShaderResourceView> srv;
            native_device_context->CSGetShaderResources(0, 1, &srv);
            com_ptr<ID3D11Resource> color;
            if (srv)
               srv->GetResource(&color);
            D3D11_TEXTURE2D_DESC color_desc;
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            game_device_data.cmaa2_replaced = GetTextureDesc(color.get(), &color_desc) && CanRunSMAA(device_data, color_desc);
         }
         if (game_device_data.cmaa2_replaced)
            return DrawOrDispatchOverrideType::Replaced;
      }
      else if (is_compute && hash == cmaa2_apply_compute_shader && game_device_data.cmaa2_replaced)
      {
         // u0 already holds a full copy of the post-ccr canvas, so if SMAA fails now the frame just goes un-antialiased.
         game_device_data.cmaa2_replaced = false;
         com_ptr<ID3D11UnorderedAccessView> uav;
         native_device_context->CSGetUnorderedAccessViews(0, 1, &uav);
         com_ptr<ID3D11Resource> canvas;
         if (uav)
            uav->GetResource(&canvas);
         if (canvas)
            RunSMAA(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, canvas.get(), canvas.get());
         return DrawOrDispatchOverrideType::Replaced;
      }
      else if (!is_compute && hash == fxaa_pixel_shader)
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> color, target;
         if (srv)
            srv->GetResource(&color);
         if (rtv)
            rtv->GetResource(&target);
         if (color && target && RunSMAA(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, color.get(), target.get()))
         {
#if DEVELOPMENT
            if (game_device_data.FirstLog(SMAAReplacedFXAA))
               LogFormatted(reshade::log::level::info, "[YRC] frame %u SMAA replaced FXAA", cb_luma_global_settings.FrameIndex);
#endif
            return DrawOrDispatchOverrideType::Replaced;
         }
#if DEVELOPMENT
         if (game_device_data.FirstLog(NativeFXAARan))
            LogFormatted(reshade::log::level::warning, "[YRC] frame %u native FXAA ran (SMAA off or unavailable)", cb_luma_global_settings.FrameIndex);
#endif
      }
      // RCAS already sharpened the SMAA output: the game's CAS only forwards its input (with the markers drawn since).
      else if (is_compute && hash == cas_compute_shader && game_device_data.smaa_ran)
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->CSGetShaderResources(0, 1, &srv);
         com_ptr<ID3D11UnorderedAccessView> uav;
         native_device_context->CSGetUnorderedAccessViews(0, 1, &uav);
         com_ptr<ID3D11Resource> source, target;
         if (srv)
            srv->GetResource(&source);
         if (uav)
            uav->GetResource(&target);
         if (source && target && AreResourcesEqual(source.get(), target.get()))
         {
            native_device_context->CopyResource(target.get(), source.get());
#if DEVELOPMENT
            if (game_device_data.FirstLog(CASReplacedByCopy))
               LogFormatted(reshade::log::level::info, "[YRC] frame %u game CAS replaced by a copy (RCAS ran with SMAA)", cb_luma_global_settings.FrameIndex);
#endif
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // The scaling CAS (render scale other than 100%) also resamples to the swapchain, so it can't become a copy: flagged,
      // its replacement drops the sharpening and only resamples.
      else if (is_compute && hash == cas_scaled_compute_shader && game_device_data.smaa_ran)
      {
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::compute, LumaConstantBufferType::LumaData, 1u);
         updated_cbuffers = true;
      }
      // Off the grade (and in games where it never grades) Core binds the default Luma data (CustomData1 = 0) and the
      // replacement stays a copy.
      else if (!is_compute && hash == aliased_passthrough_ccr_pixel_shader && g_game_profile.grades_aliased_passthrough_ccr)
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         uint4 size;
         DXGI_FORMAT format;
         GetResourceInfo(rtv.get(), size, format);
         if (IsSceneSized(device_data, size.x, size.y))
         {
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 1u);
            updated_cbuffers = true;
#if DEVELOPMENT
            game_device_data.drew_ccr = true;
            game_device_data.ccr_hash = hash;
#endif
         }
      }
      // The replacement clamps the glow source per texel only on this flag (see SampleGlowSource in Includes/Common.hlsl).
      // Luma Bloom prefilters the same source first; the native downsample still runs (Y5R's exposure meter reads it).
      else if (!is_compute && hash == g_game_profile.glow_downsample_pixel_shader)
      {
         game_device_data.scene_done = true;
         if (g_luma_bloom_enable)
            PrefilterGlowSource(native_device, native_device_context, cmd_list_data, device_data);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 1u);
         updated_cbuffers = true;
      }
      else if (!is_compute && game_device_data.glow_level0 && glow_pass0_pixel_shaders.contains(hash))
      {
         game_device_data.glow_replaced = RunLumaBloom(native_device, native_device_context, cmd_list_data, device_data);
         if (game_device_data.glow_replaced)
            return DrawOrDispatchOverrideType::Replaced;
      }
      else if (!is_compute && game_device_data.glow_replaced && hash == glow_pass1_pixel_shader)
      {
         return DrawOrDispatchOverrideType::Replaced;
      }
      // The game's sum draw (its VS, viewport, cb5 and additive blend onto the scene) with the Luma Bloom composite as its PS.
      else if (!is_compute && game_device_data.glow_replaced && glow_pass2_pixel_shaders.contains(hash))
      {
         game_device_data.glow_replaced = false;
#if DEVELOPMENT
         // The composite applies one weight (cb5[0].yzw) to pass0's rgb + alpha: exact while cb5[0].x matches it.
         if (log_frame)
         {
            com_ptr<ID3D11Buffer> cb;
            native_device_context->PSGetConstantBuffers(5, 1, &cb);
            std::vector<float> data;
            com_ptr<ID3D11Buffer> cb_copy;
            if (CopyBuffer(cb, native_device_context, data, cb_copy) && data.size() >= 4 && (data[0] != data[1] || data[0] != data[2] || data[0] != data[3]) && game_device_data.FirstLog(GlowPass2WeightsDiffer))
               LogFormatted(reshade::log::level::warning, "[YRC] frame %u glow_pass2 cb5[0] (%f %f %f %f): alpha and rgb weights differ, the Luma Bloom composite folds them", cb_luma_global_settings.FrameIndex, data[0], data[1], data[2], data[3]);
         }
#endif
         // Without the shader the frame goes without bloom: the native levels were never built.
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         uint4 size;
         DXGI_FORMAT format;
         GetResourceInfo(rtv.get(), size, format);
         if (original_draw_dispatch_func && game_device_data.bloom_all_mips_srv && size.x != 0 && size.y != 0)
         {
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, 1.f / float(size.x), 1.f / float(size.y));
            updated_cbuffers = true;
            DrawWithLumaPixelShader(native_device_context, device_data, *original_draw_dispatch_func, "YRC Bloom Composite PS"_h, game_device_data.bloom_all_mips_srv.get(), device_data.sampler_state_linear.get());
         }
         return DrawOrDispatchOverrideType::Replaced;
      }

#if DEVELOPMENT
      // Dev logger for values the DevKit can't show continuously: frames that skip the ccr (untonemapped scene), the active
      // ccr perm and its grade constants, the material exposure scale, and the first target/blend state of watched passes.
      if (!is_compute && ccr_hashes.contains(hash))
      {
         game_device_data.drew_ccr = true;
         game_device_data.ccr_hash = hash;
         if (log_frame)
         {
            com_ptr<ID3D11Buffer> cb;
            native_device_context->PSGetConstantBuffers(5, 1, &cb);
            std::vector<float> data;
            com_ptr<ID3D11Buffer> cb_copy;
            if (CopyBuffer(cb, native_device_context, data, cb_copy) && (data != game_device_data.last_logged_ccr_cb5 || hash != game_device_data.last_logged_ccr_hash))
            {
               game_device_data.last_logged_ccr_cb5 = data;
               game_device_data.last_logged_ccr_hash = hash;
               LogFormatted(reshade::log::level::info, "[YRC] frame %u ccr 0x%08X cb5 (%zu floats):", cb_luma_global_settings.FrameIndex, hash, data.size());
               for (size_t i = 0; i + 3 < data.size() && i < 64; i += 4)
                  LogFormatted(reshade::log::level::info, "[YRC]   c%zu = %.4f %.4f %.4f %.4f", i / 4, data[i], data[i + 1], data[i + 2], data[i + 3]);
            }
         }
      }
      else if (!is_compute && video_hashes.contains(hash))
      {
         game_device_data.drew_video = true;
      }
      // ASSAO prepare constants: depth unpack (cb0[1].x / (cb0[1].y - d); reversed Z in Y4R/Y5R) and the effect settings.
      // Only reached with XeGTAO off: the replaced prepare returns above.
      else if (!is_compute && hash == assao_prepare_pixel_shader && log_frame)
      {
         com_ptr<ID3D11Buffer> cb;
         native_device_context->PSGetConstantBuffers(0, 1, &cb);
         std::vector<float> data;
         com_ptr<ID3D11Buffer> cb_copy;
         // Intel ASSAO layout: c0 viewport/half-viewport pixel size, c1 DepthUnpackConsts + CameraTanHalfFOV,
         // c2 NDCToViewMul/Add, c3 per-pass offsets, c4 Viewport2xPixelSize, c5 EffectRadius/ShadowStrength/ShadowPow/ShadowClamp,
         // c6 FadeOutMul/Add, HorizonAngleThreshold, SamplingRadiusNearLimitRec, c7-c8 the rest.
         if (CopyBuffer(cb, native_device_context, data, cb_copy) && data.size() >= 36)
         {
            data.resize(36);
            if (data != game_device_data.last_logged_assao_cb0)
            {
               game_device_data.last_logged_assao_cb0 = data;
               LogFormatted(reshade::log::level::info, "[YRC] frame %u ASSAO cb0:", cb_luma_global_settings.FrameIndex);
               for (size_t i = 0; i < 36; i += 4)
                  LogFormatted(reshade::log::level::info, "[YRC]   a%zu = %f %f %f %f", i / 4, data[i], data[i + 1], data[i + 2], data[i + 3]);
            }
         }
      }
      // Materials: before the ccr, with the per-material cb1/cb2/cb11 set bound (post passes don't bind cb1).
      else if (!is_compute && log_frame && !game_device_data.drew_ccr && game_device_data.exposure_reads < max_exposure_reads)
      {
         com_ptr<ID3D11Buffer> cbs[3];
         native_device_context->PSGetConstantBuffers(1, 2, &cbs[0]);
         native_device_context->PSGetConstantBuffers(11, 1, &cbs[2]);
         std::vector<float> data;
         com_ptr<ID3D11Buffer> cb_copy;
         // Only material draws count toward the budget (shadow and depth passes come first and bind no cb1/cb2/cb11 set).
         if (cbs[0].get() && cbs[1].get() && cbs[2].get() && ++game_device_data.exposure_reads && CopyBuffer(cbs[1], native_device_context, data, cb_copy) && data.size() >= 16 * 4)
         {
            const size_t scale = g_game_profile.material_scale_register * 4;
            const std::array<float, 4> exposure = {data[24], data[scale], data[scale + 1], data[scale + 2]};
            if (std::find(game_device_data.exposure_samples.begin(), game_device_data.exposure_samples.end(), exposure) == game_device_data.exposure_samples.end())
               game_device_data.exposure_samples.push_back(exposure);
         }
      }

      // Bloom: which pixel shaders render into its scene-sized source (a pooled target), and its live constants.
      if (!is_compute && hash == g_game_profile.glow_downsample_pixel_shader)
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         com_ptr<ID3D11Resource> source;
         if (srv)
            srv->GetResource(&source);
         uint4 size;
         DXGI_FORMAT format;
         GetResourceInfo(source.get(), size, format);
         if (IsSceneSized(device_data, size.x, size.y))
            game_device_data.glow_source = source;
         if (game_device_data.FirstLog(GlowDownsampleSampler))
         {
            com_ptr<ID3D11SamplerState> sampler;
            native_device_context->PSGetSamplers(0, 1, &sampler);
            D3D11_SAMPLER_DESC sampler_desc = {};
            if (sampler)
               sampler->GetDesc(&sampler_desc);
            LogFormatted(reshade::log::level::info, "[YRC] frame %u glow downsample sampler: filter 0x%X, address %d/%d (the per-texel clamp assumes linear)", cb_luma_global_settings.FrameIndex, sampler_desc.Filter, sampler_desc.AddressU, sampler_desc.AddressV);
         }
      }
      else if (!is_compute && log_frame && game_device_data.glow_source && !game_device_data.logged_glow_source_writers.contains(hash))
      {
         com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
         native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], nullptr);
         for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
         {
            com_ptr<ID3D11Resource> target;
            if (rtvs[i])
               rtvs[i]->GetResource(&target);
            if (target && target == game_device_data.glow_source)
            {
               game_device_data.logged_glow_source_writers.insert(hash);
               LogFormatted(reshade::log::level::info, "[YRC] frame %u glow source written by PS 0x%08X (RT %u of the draw)", cb_luma_global_settings.FrameIndex, hash, i);
               break;
            }
         }
      }
      // Y5R ps_down_sample_4x4: its tap offsets set how much it re-blurs the glow level (see bloom_level_sigma).
      if (!is_compute && hash == glow_reexpand_pixel_shader && game_device_data.FirstLog(GlowReexpandOffsets))
      {
         com_ptr<ID3D11Buffer> vs_cb;
         native_device_context->VSGetConstantBuffers(7, 1, &vs_cb);
         std::vector<float> data;
         com_ptr<ID3D11Buffer> cb_copy;
         if (CopyBuffer(vs_cb, native_device_context, data, cb_copy) && data.size() >= 4)
            LogFormatted(reshade::log::level::info, "[YRC] frame %u ps_down_sample_4x4 VS cb7[0]: (%f %f %f %f) (256x128 source texel = %f %f)", cb_luma_global_settings.FrameIndex, data[0], data[1], data[2], data[3], 1.f / 256.f, 1.f / 128.f);
      }
      const bool is_glow_pass0 = !is_compute && glow_pass0_pixel_shaders.contains(hash);
      if (log_frame && (is_glow_pass0 || (!is_compute && glow_pass2_pixel_shaders.contains(hash))))
      {
         // pass0: cb5[0] luma/rgb threshold, cb5[1] threshold scale, cb5[2] source scale, cb11[0].y & 8 = scene threshold on.
         // pass2: cb5[0].x luma term, .yzw rgb scale of the 5-level sum.
         com_ptr<ID3D11Buffer> cbs[2];
         native_device_context->PSGetConstantBuffers(5, 1, &cbs[0]);
         native_device_context->PSGetConstantBuffers(11, 1, &cbs[1]);
         std::vector<float> data, cb11_data;
         com_ptr<ID3D11Buffer> cb_copy;
         if (CopyBuffer(cbs[0], native_device_context, data, cb_copy) && data.size() >= 12)
         {
            data.resize(12);
            if (is_glow_pass0 && CopyBuffer(cbs[1], native_device_context, cb11_data, cb_copy) && cb11_data.size() >= 4)
               data.insert(data.end(), cb11_data.begin(), cb11_data.begin() + 4);
            auto& last_logged = game_device_data.last_logged_glow_cbs[is_glow_pass0 ? 0 : 1];
            if (data != last_logged)
            {
               last_logged = data;
               LogFormatted(reshade::log::level::info, "[YRC] frame %u %s cb5: (%f %f %f %f) (%f %f %f %f) (%f %f %f %f)", cb_luma_global_settings.FrameIndex, is_glow_pass0 ? "glow_pass0" : "glow_pass2", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11]);
               if (data.size() >= 16)
               {
                  uint32_t flags[4];
                  std::memcpy(flags, &data[12], sizeof(flags));
                  LogFormatted(reshade::log::level::info, "[YRC]   glow_pass0 cb11[0]: 0x%X 0x%X 0x%X 0x%X (scene threshold %s)", flags[0], flags[1], flags[2], flags[3], (flags[1] & 8u) ? "on" : "off");
               }
            }
         }
      }

      // glow_pass1 (8 draws: 4 levels, H then V): its 6 tap weights (PS cb5[1..3], taps at +-0.5/1.5/2.5 x VS cb7[0].xy)
      // and sizes, from which the Luma Bloom widths are set.
      if (!is_compute && hash == glow_pass1_pixel_shader && log_frame && game_device_data.glow_pass1_draws < 8)
      {
         const uint32_t draw = game_device_data.glow_pass1_draws++;
         com_ptr<ID3D11Buffer> ps_cb, vs_cb;
         native_device_context->PSGetConstantBuffers(5, 1, &ps_cb);
         native_device_context->VSGetConstantBuffers(7, 1, &vs_cb);
         std::vector<float> ps_data, vs_data;
         com_ptr<ID3D11Buffer> cb_copy;
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         uint4 target_size, source_size;
         DXGI_FORMAT format;
         GetResourceInfo(rtv.get(), target_size, format);
         GetResourceInfo(srv.get(), source_size, format);
         if (CopyBuffer(ps_cb, native_device_context, ps_data, cb_copy) && ps_data.size() >= 16 && CopyBuffer(vs_cb, native_device_context, vs_data, cb_copy) && vs_data.size() >= 4)
         {
            std::vector<float> data(ps_data.begin(), ps_data.begin() + 16);
            data.insert(data.end(), vs_data.begin(), vs_data.begin() + 4);
            data.insert(data.end(), {float(target_size.x), float(target_size.y), float(source_size.x), float(source_size.y)});
            if (data != game_device_data.last_logged_glow_pass1[draw])
            {
               game_device_data.last_logged_glow_pass1[draw] = data;
               LogFormatted(reshade::log::level::info, "[YRC] frame %u glow_pass1 #%u %ux%u <- %ux%u: cb5 (%f %f %f %f) (%f %f %f %f) (%f %f %f %f) (%f %f %f %f), VS cb7[0] (%f %f %f %f)", cb_luma_global_settings.FrameIndex, draw, target_size.x, target_size.y, source_size.x, source_size.y, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17], data[18], data[19]);
            }
         }
      }

      // DoF composites: alpha = saturate(depth term) * cb5[0].z (pass2_mask also * mask * cb5[1].x); a scale above 1 would
      // extrapolate the blend into the fp16 scene.
      if (!is_compute && log_frame && dof_composite_pixel_shaders.contains(hash))
      {
         com_ptr<ID3D11Buffer> cb;
         native_device_context->PSGetConstantBuffers(5, 1, &cb);
         std::vector<float> data;
         com_ptr<ID3D11Buffer> cb_copy;
         if (CopyBuffer(cb, native_device_context, data, cb_copy) && data.size() >= 8)
         {
            data.resize(8);
            if (data != game_device_data.last_logged_dof_cb5)
            {
               game_device_data.last_logged_dof_cb5 = data;
               LogFormatted(reshade::log::level::info, "[YRC] frame %u DoF composite 0x%08X cb5: (%f %f %f %f) (%f %f %f %f)", cb_luma_global_settings.FrameIndex, hash, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
            }
         }
      }

      // Blended-effect trap: the first draw of every pixel shader that blends into a scene-sized target, with the frame
      // index. Effects that break on the fp16 chain (alpha above 1 extrapolates the blend) show up as new lines at the time
      // they appear on screen; name the hashes offline from the game's shader archives.
      if (!is_compute && game_device_data.seen_blend_hashes.insert(hash).second)
      {
         com_ptr<ID3D11BlendState> blend_state;
         FLOAT blend_factor[4];
         UINT sample_mask;
         native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
         D3D11_BLEND_DESC blend = {};
         if (blend_state)
            blend_state->GetDesc(&blend);
         const D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend.RenderTarget[0];
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         uint4 size;
         DXGI_FORMAT format;
         GetResourceInfo(rtv.get(), size, format);
         if (rt_blend.BlendEnable && IsSceneSized(device_data, size.x, size.y))
            LogFormatted(reshade::log::level::info, "[YRC] frame %u blended PS 0x%08X: target format %u, color %d/%d op %d, alpha %d/%d, ccr drawn before: %d", cb_luma_global_settings.FrameIndex, hash, format, rt_blend.SrcBlend, rt_blend.DestBlend, rt_blend.BlendOp, rt_blend.SrcBlendAlpha, rt_blend.DestBlendAlpha, game_device_data.drew_ccr);
      }

      // Every pass writing the offscreen targets upgraded for the DoF: each one now sees fp16 values above 1.
      if (!is_compute && log_frame && game_device_data.checked_custom_size_hashes.insert(hash).second)
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         uint4 size;
         DXGI_FORMAT format;
         GetResourceInfo(rtv.get(), size, format);
         if (std::ranges::any_of(g_game_profile.dof_custom_sizes, [&](const uint2& s)
                { return s.x == size.x && s.y == size.y; }))
            LogFormatted(reshade::log::level::info, "[YRC] frame %u custom-size target %ux%u format %u written by PS 0x%08X", cb_luma_global_settings.FrameIndex, size.x, size.y, format, hash);
      }

      if (const auto watched = watched_hashes.find(hash); watched != watched_hashes.end() && game_device_data.logged_watched_hashes.insert(hash).second)
      {
         uint4 size;
         DXGI_FORMAT format;
         if (is_compute)
         {
            com_ptr<ID3D11UnorderedAccessView> uav;
            native_device_context->CSGetUnorderedAccessViews(0, 1, &uav);
            GetResourceInfo(uav.get(), size, format);
         }
         else
         {
            com_ptr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
            GetResourceInfo(rtv.get(), size, format);
         }
         // t0 is the color input of the DoF and video passes: an 8-bit source there clips before the pass even runs.
         uint4 source_size = {};
         DXGI_FORMAT source_format = DXGI_FORMAT_UNKNOWN;
         D3D11_BLEND_DESC blend = {};
         if (!is_compute)
         {
            com_ptr<ID3D11ShaderResourceView> srv;
            native_device_context->PSGetShaderResources(0, 1, &srv);
            GetResourceInfo(srv.get(), source_size, source_format);

            com_ptr<ID3D11BlendState> blend_state;
            FLOAT blend_factor[4];
            UINT sample_mask;
            native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
            if (blend_state.get())
               blend_state->GetDesc(&blend);
         }
         const D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend.RenderTarget[0];
         LogFormatted(reshade::log::level::info, "[YRC] frame %u first %s 0x%08X: target %ux%u format %u, t0 %ux%u format %u, blend %d (color %d/%d, alpha %d/%d), ccr drawn before: %d", cb_luma_global_settings.FrameIndex, watched->second, hash, size.x, size.y, format, source_size.x, source_size.y, source_format, rt_blend.BlendEnable, rt_blend.SrcBlend, rt_blend.DestBlend, rt_blend.SrcBlendAlpha, rt_blend.DestBlendAlpha, game_device_data.drew_ccr);
      }

#endif

      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.depth_srv.reset();
      game_device_data.cmaa2_replaced = false;
      game_device_data.smaa_ran = false;
      game_device_data.assao_replaced = false;
      game_device_data.glow_replaced = false;
      game_device_data.glow_level0.reset();
      game_device_data.scene_done = false;
      // Turning Luma Bloom off gives its textures back; they are rebuilt on demand.
      if (!g_luma_bloom_enable && game_device_data.glow_srvs[0])
         game_device_data.ReleaseGlowTextures();
      // Turning XeGTAO off gives its scratch back; it is rebuilt on demand.
      if (!g_gtao_enable && game_device_data.gtao_width != 0)
         game_device_data.ReleaseGTAOScratch();

#if DEVELOPMENT
      const uint32_t frame = cb_luma_global_settings.FrameIndex;

      // Frames without the ccr reach the display untonemapped (the fp16 scene can exceed paper white many times).
      if (!game_device_data.drew_ccr)
      {
         if (game_device_data.previous_frame_drew_ccr)
            LogFormatted(reshade::log::level::warning, "[YRC] frame %u: no ccr pass (video %d) - scene not tonemapped", frame, game_device_data.drew_video);
         game_device_data.frames_without_ccr++;
      }
      else if (!game_device_data.previous_frame_drew_ccr)
      {
         LogFormatted(reshade::log::level::info, "[YRC] frame %u: ccr back (0x%08X) after %u frames without", frame, game_device_data.ccr_hash, game_device_data.frames_without_ccr);
         game_device_data.frames_without_ccr = 0;
      }

      if (!game_device_data.exposure_samples.empty()) // Only sampled on log frames
      {
         std::string line = std::format("[YRC] frame {} material cb2[6].x, cb2[{}].xyz:", frame, g_game_profile.material_scale_register);
         for (const auto& e : game_device_data.exposure_samples)
            line += std::format(" ({:.3f} {:.3f} {:.3f} {:.3f})", e[0], e[1], e[2], e[3]);
         reshade::log::message(reshade::log::level::info, line.c_str());
      }

      if (const uint32_t patches = y5_material_curve_patches; patches != game_device_data.logged_material_curve_patches)
      {
         game_device_data.logged_material_curve_patches = patches;
         LogFormatted(reshade::log::level::info, "[YRC] frame %u: Y5R material tone curve extended in %u shaders", frame, patches);
      }

      game_device_data.previous_frame_drew_ccr = game_device_data.drew_ccr;
      game_device_data.drew_ccr = false;
      game_device_data.drew_video = false;
      game_device_data.exposure_samples.clear();
      game_device_data.exposure_reads = 0;
      game_device_data.glow_pass1_draws = 0;
#endif
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Yakuza 3 Remastered\", \"Yakuza 4 Remastered\" and \"Yakuza 5 Remastered\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR and replaces the game's bloom with Luma Bloom, its CMAA2/FXAA with SMAA and its SSAO with XeGTAO.\n"
         "Enable Anti-Aliasing and Ambient Occlusion in the game's graphics settings for SMAA and XeGTAO to apply.\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70 + 18, 134 + 18, 0, 255));
      static const std::string donation_link = std::string("Buy DristoforColumb a Coffee on ko-fi ") + std::string(ICON_FK_OK);
      if (ImGui::Button(donation_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      ImGui::PopStyleColor(3);

      ImGui::NewLine();
      static const std::string social_link = std::string("Join our \"HDR Den\" Discord ") + std::string(ICON_FK_SEARCH);
      if (ImGui::Button(social_link.c_str()))
      {
         // Unique link for Luma's HDR Den (tracks the origin of people joining); do not share for other purposes.
         static const std::string discord_link = std::string("https://discord.gg/J9fM") + std::string("3EVuEZ");
         ShellExecuteA(nullptr, "open", discord_link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      static const std::string contributing_link = std::string("Contribute on Github ") + std::string(ICON_FK_FILE_CODE);
      if (ImGui::Button(contributing_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nMain:"
                  "\nDristoforColumb"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nRenoDX (HDR tonemap method)"
                  "\nDICE (HDR tonemapper)"
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nXeGTAO (Intel)"
                  "\nAMD FidelityFX (RCAS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Yakuza Remastered Collection Luma mod", "", 1);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // b8g8r8a8_unorm backbuffer -> r16g16b16a16_float
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      // The scene RT and every scene-sized post target (CMAA2/CAS/resample/fade) are 8-bit UNORM. The lower bloom levels
      // (256x128 and smaller) stay 8-bit on purpose: the vanilla [0,1] bound is part of the look.
      texture_upgrade_formats = {
         reshade::api::format::b8g8r8a8_unorm,
         reshade::api::format::b8g8r8a8_typeless,
         reshade::api::format::r8g8b8a8_unorm,
         reshade::api::format::r8g8b8a8_typeless,
      };
      // The DoF runs before the ccr on offscreen scene copies and blurs (sizes per game, see "DetectGame") that would clip
      // highlights inside the blurred area, so those sizes are upgraded too. The top bloom level shares 512x256, so
      // glow_pass0/1 saturate to keep the vanilla bloom bound. Hash-based mirrors don't help: they go through the same
      // size filter.
      // Below 100% render scale (and under Y5R's dynamic resolution) the scene chain runs at a scaled size up to the AA and a
      // resample (Y3R 0xFC6DEC24) brings it to the swapchain: the aspect ratio filter catches every scale (see
      // IsSceneSized). The engine's fixed-size targets (512x256, 512x512, 1024x1024, 256x256) are not 16:9.
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px | (uint32_t)TextureFormatUpgrades2DSizeFilters::CustomSize;
      g_game_profile = DetectGame();
      texture_format_upgrades_2d_custom_sizes = g_game_profile.dof_custom_sizes;

      game = new GameYakuzaRC();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
