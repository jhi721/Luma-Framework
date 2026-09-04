// Mass Effect (2007) - Luma HDR mod (Unreal Engine 3 2007, 32-bit, DX9 -> D3D11 via dgVoodoo2). Clone of the Medal
// of Honor: Airborne port: same engine year, same wrapper, same UberPostProcessBlend / FGammaCorrection pair with the
// same cbuffer names, only the hashes and the cb4 rows differ (see Luma_ME1_Tonemap.hlsl).
// Hashes are dgVoodoo-TRANSLATED and change per wrapper build: 2.87.3 (ps_5_0) and 2.81.3 (ps_4_0) are both keyed.
// Any other build needs a re-dump; the twins were recovered by an opcode+cbuffer-row fingerprint over the two dumps
// (see NOTES.md), which is also how the sibling MoHA and TW2 ports key their second build.
// The post chain is fp16 with scene DEPTH in the alpha: no depth SRV, no motion vectors, so DLSS/DLAA is impossible.
// Measured frame (devkit, gameplay): UberPostProcessBlend reads scene A (fp16) + the quarter-res blur and writes scene
// B (fp16); the engine's copy pass 0x1E37D75B mirrors B back into A; FGammaCorrection then reads A and writes the
// 8-bit canvas the HUD blends onto. Unlike MoHA the two passes CHAIN every frame, so the HDR work is split: the uber
// replacement leaves LINEAR HDR in the fp16 intermediate, the gamma-correction replacement owns the canvas encode.

// No DEVELOPMENT auto-debugger MessageBox on DLL attach: invisible under borderless/fullscreen and it blocks the
// loader (ReShade times out the addon load -> error 1114). Same failure as BL2/TW2 under dgVoodoo.
#define DISABLE_AUTO_DEBUGGER 1

#define GAME_MASS_EFFECT 1

#define ENABLE_NGX 0 // NGX is x64-only and the game is 32-bit (and there are no motion vectors anyway)
#define ENABLE_FIDELITY_SK 0
#define GEOMETRY_SHADER_SUPPORT 0
// The game ships no AA at all (no option, no post AA pass in the dump, sampleCount 1 everywhere), so SMAA adds
// rather than replaces. Core auto-registers the 6 "SMAA ..." passes from Luma_SMAA_impl.hlsl.
#define ENABLE_SMAA 1
// Luma's multi-scale HDR bloom pyramid REPLACES the game's own quarter-res bright-pass glow (which the replaced
// gather pass then stops writing). Core auto-registers the 4 "Bloom ..." passes from Luma_Bloom_impl.hlsl.
#define ENABLE_BLOOM 1
// SMAA runs POST-final-grade, before the HUD draws on the canvas, via the post-draw callback. Outside DEVELOPMENT
// this define is what makes "original_draw_dispatch_func" non-null; without it the callback silently never fires.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1

#include "..\..\Core\core.hpp"
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)
#include <unordered_set>

// Both replaced, both run every gameplay frame (uber first, gamma correction last). Separate register maps.
static constexpr uint32_t kUberPostHash = 0xAC8341E0;        // UberPost_0xAC8341E0.ps_5_0.hlsl: HDR grade -> fp16 intermediate
static constexpr uint32_t kGammaCorrectionHash = 0x17CE0932; // GammaCorrection_0x17CE0932.ps_5_0.hlsl: canvas encode (the FINAL pass)
// The engine's intermediate copy (scene B -> A), run after every post pass. Not replaced; only its gamma row is
// dumped in DEVELOPMENT, because a non-1 exponent there would make it the pass that gamma-encodes the frame.
static constexpr uint32_t kCopyPassHash = 0x1E37D75B;

// UE3 DOFAndBloomGather, REPLACED: it writes the game's bloom and its DoF blur SUMMED into one quarter-res target,
// so the vanilla glow can only be switched off inside this shader — anything downstream takes DoF with it.
static constexpr uint32_t kDofBloomGatherHash = 0x56854256;  // DofBloomGather_0x56854256.ps_5_0.hlsl (QualityBloom=TRUE, 16 taps)
static constexpr uint32_t kDofBloomGather4Hash = 0x28F8DB16; // DofBloomGather4_0x28F8DB16.ps_5_0.hlsl (QualityBloom=FALSE, 4 taps)
// UE3 FilterPixelShader, 9 taps, NOT replaced: blurs the gather's DoF+bloom buffer twice (H then V). Its weights (PS
// cb4[10..18]) and offsets (VS cb4[25..29]) define the vanilla glow radius the Luma pyramid is calibrated against.
static constexpr uint32_t kBloomFilterHash = 0x6A1129DF;

// The same passes under dgVoodoo 2.81.3, which emits ps_4_0 and so hashes differently. Dump-verified: the instruction
// stream, interpolators, resource slots and cb4 rows match their 2.87.3 twin, differing only in temp-register
// numbering, `centroid` on the input declarations and (in three of the UI shaders) where the saturating moves sit.
// The shader-side twins are thin `#include` forwarders named <Name>_0x<hash>.ps_4_0.hlsl; only the hashes the C++
// keys on need a constant here, because a replacement is bound by FILE NAME (so the UI and video twins need none).
// The 4-tap gather needed its own collection run: it only compiles with [SystemSettings] QualityBloom=FALSE.
static constexpr uint32_t kUberPostHash_v281 = 0x786BC3B3;
static constexpr uint32_t kGammaCorrectionHash_v281 = 0x3BEF1CD6;
static constexpr uint32_t kCopyPassHash_v281 = 0xDDEAEB7C;
static constexpr uint32_t kDofBloomGatherHash_v281 = 0x4B65EEAE;
static constexpr uint32_t kDofBloomGather4Hash_v281 = 0xAA369C00;
static constexpr uint32_t kBloomFilterHash_v281 = 0x464E33BB;

// Luma bloom pyramid mip 0, read by the grade replacements at register(t6): clear of the two slots those
// shaders actually declare (t0 scene, t1 blur).
static constexpr uint32_t kLumaBloomSlot = 6;
// One sigma per mip, count taken FROM the array so the two cannot drift (MELE). MoHA's set (BL2's with the last octave
// halved): six mips, 1/2..1/64 res, blended 0.5/0.5 up the chain (weights 1/2, 1/4, ... summing to 1), so the pyramid
// is energy-preserving and the BloomScaleLive gain alone makes Bloom Intensity 1 vanilla STRENGTH.
// The wider SHAPE is deliberate and was chosen in-game (2026-09-02) over a one-mip pyramid matching the game's own
// kernel (measured: isotropic Gaussian, sigma 2.7 quarter-res texels = 0.28% of the scene width; derivation in
// NOTES.md). That A/B is settled, so only the chosen set ships.
static float g_bloom_sigmas[] = {1.5f, 2.f, 2.f, 2.f, 1.f, 0.5f};

// User settings, persisted in the [Luma] config section (LoadConfigs) unless noted otherwise.
static bool g_hide_ui = false; // hide the game's HUD (for clean screenshots); session-only, never persisted
#if ENABLE_SMAA
static bool g_smaa_enable = true;
static bool g_smaa_predication = true;      // predicate SMAA on geometry, using the depth in the scene buffer's alpha
static float g_smaa_pred_tolerance = 0.02f; // plane deviation counted as a full edge, as a fraction of view depth
// RCAS sharpen on the SMAA output, opt-in at 0 (BL2/TW2 precedent): how much sharpening is wanted is a
// preference, not a target. At 0 the pass does not run and its full-resolution intermediate is never allocated.
static float g_rcas_sharpness = 0.f;
#if DEVELOPMENT
// Calibration aid for g_smaa_pred_tolerance, the only free parameter here: predication's effect is the ABSENCE of
// smearing, which the eye reads badly and worse in motion, so judge the mask itself instead of the frame.
static bool g_smaa_pred_debug = false;   // show the predication mask instead of the antialiased frame
static bool g_smaa_pred_measure = false; // one-shot: log the mask's coverage above 0.5 and percentiles
#endif
#if DEVELOPMENT
// One-shot: dump the grade-relevant cb4 rows of the copy, uber and gamma passes, disarmed by the gamma pass (the
// last of the three). On demand rather than on a frame cadence - the values are settled (see the
// GammaColorScaleAndInverse maps) and each dump is a blocking Map.
static bool g_dump_pass_cb = false;
#endif
#endif

// Luma HDR bloom. Mirrored into GameSettings.LumaBloomEnable, which both the grade (composite) and the replaced
// gather (stop writing the vanilla glow) read — so this single switch really swaps one bloom for the other.
static bool g_luma_bloom_enable = true;

struct MassEffectGameDeviceData final : public GameDeviceData
{
   // Repaired blend states, keyed by the ORIGINAL desc (DXHR precedent). Keying by desc rather than by the
   // source state's pointer means a released state can't leave a stale key that a later allocation reuses.
   struct BlendDescCompare
   {
      bool operator()(const D3D11_BLEND_DESC& a, const D3D11_BLEND_DESC& b) const
      {
         return memcmp(&a, &b, sizeof(D3D11_BLEND_DESC)) < 0;
      }
   };
   std::map<D3D11_BLEND_DESC, ComPtr<ID3D11BlendState>, BlendDescCompare> fixed_blend_states;

   bool has_drawn_tonemap = false; // the uber pass ran this frame (bloom injected, scene captured)
   bool has_drawn_final = false;   // the gamma-correction pass ran this frame (canvas captured, SMAA done)
   // Wrapper-build telemetry: an unkeyed dgVoodoo build fails SILENTLY (format-keyed upgrades still fire: fp16
   // canvas, no replacements). Latched across frames, reported once after warmup (OnPresent).
   bool ever_matched_final_pass = false;
   bool build_check_done = false;
   uint32_t frames_presented = 0;
#if DEVELOPMENT
   // Format-upgrade diagnostics, one-shot per DEVICE rather than per process: dgVoodoo recreates the device on
   // resolution and display-mode changes, which is exactly when the fp16 mirror is worth re-reading.
   bool diag_logged_gather = false;
   bool diag_logged_rt = false;
   // Vanilla bloom kernel capture, one-shot: the two filter draws of one frame (H then V) and the gather's constants.
   uint32_t diag_filter_dumps = 0;
   // Every pixel shader seen drawing onto the canvas after the gamma-correction pass, i.e. the HUD family. Each one
   // lands on the fp16 mirror without the 8-bit write clamp, so each needs a saturating replacement (UI_GFx*): logged
   // once per hash so a permutation without one is caught in the log rather than as a 10k-nit HUD element.
   // Only UNREPLACED shaders can appear: a hash-replaced draw reports is_custom_pass, which the net's gate rejects.
   // So every line this logs is a permutation still to handle.
   std::unordered_set<uint32_t> diag_post_final_ps;
#endif
   // Deferred constant-buffer readback (MoHA/MELE shape): copy at the draw, map the copy made two frames earlier with a
   // non-blocking Map, because the synchronous form would stall the GPU every frame.
   struct DeferredCBRing
   {
      static constexpr uint32_t kSlots = 3;
      ComPtr<ID3D11Buffer> staging[kSlots];
      uint32_t bytes = 0;
      uint32_t writes = 0;
      // One advance per frame, re-armed at Present: a second capture in the same frame would push the oldest slot out
      // before the GPU is done with it, so the non-blocking Map would fail forever and the value would never update.
      // Enforced here rather than at the call sites, so a new consumer cannot forget it (the gather draws more than
      // once per frame in some scenes).
      bool advanced_this_frame = false;
   };
   // The gamma-correction pass's inverse display gamma (cb4[11].x, measured 0.625 = 1/1.6 here; the uber and copy
   // exponents are 1.0). It lives in a DIFFERENT pass than the grade that needs it for the SDR reference, so it
   // travels through GameSettings with a few frames of latency. Meaningless until its _valid flag is set.
   DeferredCBRing gamma_cb_ring;
   float gamma_inverse_live = 0.f;
   bool gamma_inverse_valid = false;
   // The gather's BloomScale (cb4[11].x, measured 0.1), same deferred scheme: it is per post-process volume in UE3, so
   // it can change per area. Published as GameSettings.BloomScaleLive.
   DeferredCBRing bloom_cb_ring;
   float bloom_scale_live = 0.f;
   // Validity is a flag, NOT the sign of the value: a post-process volume with vanilla bloom off reports BloomScale 0,
   // which is a real reading and must reach the shader (otherwise Luma bloom keeps glowing on the previous zone's gain).
   bool bloom_scale_valid = false;

   // The canvas the final color pass wrote into this frame, captured from its bound RTV. Consumed by Hide UI
   // (see OnDrawOrDispatch) and by the SMAA hook. Released every Present.
   ComPtr<ID3D11Resource> canvas_res;

   // Non-owning view onto core's DrawBloom mip 0 (AddRef'd by DrawBloom; the pyramid itself is core-managed and
   // released with the swapchain). Rebuilt every frame the feature is on.
   ComPtr<ID3D11ShaderResourceView> srv_luma_bloom;

   // The scene buffer captured from t0 of the hooked final pass. Its ALPHA carries linear depth, which is what
   // SMAA predication reads. Released every Present.
   ComPtr<ID3D11ShaderResourceView> srv_scene;

#if ENABLE_SMAA
   // ---- SMAA (TW2/BL2 shape, see RunPostFinalGradeSMAA) ----
   // Metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale, 0, 0, 0).
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   uint32_t smaa_metrics_w = 0, smaa_metrics_h = 0;
   float smaa_metrics_pred_scale = -1.f;
   uint32_t smaa_core_w = 0, smaa_core_h = 0;
   // SRV-readable snapshot of the canvas; the chain writes the canvas, so it must sample this copy instead.
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;
   uint32_t smaa_temps_w = 0, smaa_temps_h = 0;

   // SMAA predication: srv_scene's alpha turned into an edge-ness mask by the depth-extract CS.
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;
   uint32_t pred_w = 0, pred_h = 0;

   void ReleasePredicationScratch()
   {
      uav_pred.reset();
      srv_pred.reset();
      tex_pred.reset();
      pred_w = pred_h = 0;
   }

   // RCAS. The intermediate exists ONLY while sharpening is on: with the slider at 0 the SMAA chain renders
   // straight into the canvas, which saves both this full-resolution copy and a write-back.
   ComPtr<ID3D11Buffer> cb_sharpen;
   uint32_t sharpen_w = 0, sharpen_h = 0;
   float sharpen_amount = -1.f;
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;
   uint32_t smaa_out_w = 0, smaa_out_h = 0;

   void ReleaseSharpenScratch()
   {
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_smaa_out.reset();
      smaa_out_w = smaa_out_h = 0;
   }

   // Turning the feature off must give the address space back: this snapshot alone is ~66 MB at 4K (fp16 canvas), with
   // core's own SMAA intermediates on top. STOCK MassEffect.exe is NOT large-address-aware (PE Characteristics 0x0102,
   // measured on an untouched copy), so the ceiling is 2 GB. MEUITM and the community patchers set the bit and lift it
   // to 4 GB - which the 4K texture packs then exhaust anyway (NOTES.md). OnInit probes the real ceiling and warns.
   void ReleaseSMAAScratch()
   {
      srv_input.reset();
      tex_input.reset();
      smaa_temps_w = smaa_temps_h = 0;
      ReleasePredicationScratch();
      ReleaseSharpenScratch();
   }
#endif
};

class MassEffect final : public Game
{
   // Pass identity by shader hash, folding both supported dgVoodoo builds (2.87.3 ps_5_0 + 2.81.3 ps_4_0). Same shape
   // as the sibling ports on this wrapper (MoHA ContainsPixelShader, TW2 IsTonemap, BL2 IsBL2Tonemap).
   static bool ContainsPixelShader(const ShaderHashesList<OneShaderPerPipeline>& hashes, uint32_t hash, uint32_t hash_v281)
   {
      return hashes.Contains(hash, reshade::api::shader_stage::pixel) || hashes.Contains(hash_v281, reshade::api::shader_stage::pixel);
   }

   static bool IsDofBloomGather(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kDofBloomGatherHash, kDofBloomGatherHash_v281) || ContainsPixelShader(hashes, kDofBloomGather4Hash, kDofBloomGather4Hash_v281);
   }

   static bool IsUberPostPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kUberPostHash, kUberPostHash_v281);
   }

   // The last colour pass before the HUD: it writes the 8-bit canvas (measured), so the canvas capture, Hide UI and
   // SMAA all key on it.
   static bool IsFinalColorPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kGammaCorrectionHash, kGammaCorrectionHash_v281);
   }

   static MassEffectGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MassEffectGameDeviceData*>(device_data.game);
   }

   // Named injected shaders live in unordered_maps the render thread otherwise only reads: look them up with
   // "find" (operator[] would default-insert on a miss, mutating a map core's draw helpers read concurrently).
   template <typename ShaderMap>
   static auto FindShader(const ShaderMap& shaders, uint32_t name)
   {
      const auto it = shaders.find(name);
      return it != shaders.end() ? it->second.get() : nullptr;
   }
   template <typename ShaderMap>
   static bool AllShadersReady(const ShaderMap& shaders, std::initializer_list<uint32_t> names)
   {
      for (const uint32_t name : names)
      {
         if (FindShader(shaders, name) == nullptr)
            return false;
      }
      return true;
   }

   static bool CreateImmutableCB(ID3D11Device* device, const void* data, UINT size, ComPtr<ID3D11Buffer>& out)
   {
      out.reset();
      D3D11_BUFFER_DESC bd = {};
      bd.ByteWidth = size;
      bd.Usage = D3D11_USAGE_IMMUTABLE;
      bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      D3D11_SUBRESOURCE_DATA sd = {};
      sd.pSysMem = data;
      return SUCCEEDED(device->CreateBuffer(&bd, &sd, out.put()));
   }

   // The SMAA snapshot must pass the canvas' live format: CopyResource requires source and destination formats
   // to match.
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>& out, DXGI_FORMAT format)
   {
      out.reset();
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = w;
      td.Height = h;
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = format;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = bind_flags;
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out.put()));
   }

   // One cbuffer row from the copy made two frames ago. False when there is nothing to read yet: fresh ring, failed
   // allocation, or a slot still in flight.
   static bool ReadCBRowDeferred(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Buffer* cb,
      MassEffectGameDeviceData::DeferredCBRing& ring, uint32_t row, float out[4])
   {
      if (cb == nullptr)
         return false;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (row + 1) * 16)
         return false;

      if (ring.bytes != bd.ByteWidth)
      {
         D3D11_BUFFER_DESC sd = {};
         sd.ByteWidth = bd.ByteWidth;
         sd.Usage = D3D11_USAGE_STAGING;
         sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         for (auto& s : ring.staging)
         {
            s.reset();
            if (FAILED(native_device->CreateBuffer(&sd, nullptr, s.put())))
            {
               for (auto& r : ring.staging) // drop a partial allocation rather than run on half a ring
                  r.reset();
               ring.bytes = 0;
               return false;
            }
         }
         ring.bytes = bd.ByteWidth;
         ring.writes = 0;
      }

      if (ring.advanced_this_frame)
         return false; // see DeferredCBRing::advanced_this_frame

      constexpr uint32_t kSlots = MassEffectGameDeviceData::DeferredCBRing::kSlots;
      native_device_context->CopyResource(ring.staging[ring.writes % kSlots].get(), cb);
      ring.writes++;
      ring.advanced_this_frame = true;
      if (ring.writes < kSlots)
         return false; // nothing old enough to read yet

      // The slot about to be overwritten next is the oldest one, i.e. the copy issued kSlots frames ago.
      ID3D11Buffer* oldest = ring.staging[ring.writes % kSlots].get();
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(oldest, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return false; // still in flight; try again next frame rather than blocking
      std::memcpy(out, (const uint8_t*)mapped.pData + (size_t)row * 16, 16);
      native_device_context->Unmap(oldest, 0);
      return true;
   }

   // Track one value out of dgVoodoo's PS constant mirror (b4) at the draw of the pass that owns it, deferred so the
   // Map never stalls. Both consumers happen to read row 11 - the row is passed in because it means a different thing
   // per pass (see the register maps in Luma_ME1_Tonemap.hlsl), not because they share a constant. Implausible data is
   // rejected rather than let through to the frame: the previous good value keeps the picture stable.
   static void TrackCB4Row(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context,
      MassEffectGameDeviceData::DeferredCBRing& ring, uint32_t row_index, float min_valid, float max_valid, float* out, bool* out_valid)
   {
      ComPtr<ID3D11Buffer> cb;
      native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      float row[4];
      if (!ReadCBRowDeferred(native_device, native_device_context, cb.get(), ring, row_index, row))
         return;
      if (row[0] >= min_valid && row[0] <= max_valid)
      {
         *out = row[0];
         *out_valid = true; // separate from the value: min_valid can legitimately BE 0 (bloom)
      }
   }

   // dgVoodoo sometimes leaves blending ENABLED on a secondary render target while RT0 has it off; D3D9 has one
   // global blend state and only per-RT write masks, so the game never asked for it and that target is corrupted.
   // Diagnosed in TW2 on the same wrapper (water PS 0xDA16C815: RT1 is the linear depth fog reads). Repair = copy
   // RT0's blend fields onto the offenders, keeping write masks; the inverse shape is only logged, since enabling
   // blending could only add damage. Returns true iff it ran the original draw itself (caller returns Replaced).
   static bool FixImpossiblePerRTBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MassEffectGameDeviceData& gd, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, std::function<void()>* original_draw_dispatch_func)
   {
      // Our own injected passes set their blend state deliberately. Re-issuing the draw is the only way to apply
      // a different state, so without that callback there is nothing to do.
      if (is_custom_pass || (stages & reshade::api::shader_stage::pixel) == 0 || original_draw_dispatch_func == nullptr)
         return false;

      ComPtr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask = 0;
      native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
      if (!blend_state)
         return false; // no state object = default (blending off everywhere)

      D3D11_BLEND_DESC bd;
      blend_state->GetDesc(&bd);
      if (!bd.IndependentBlendEnable)
         return false; // one state for all targets: already D3D9-shaped

      const bool rt0_blending = bd.RenderTarget[0].BlendEnable != FALSE;
#if !DEVELOPMENT
      // Only the "RT0 off, RTn on" shape is ever repaired, so outside DEVELOPMENT (where the inverse shape is
      // logged) a blending RT0 has nothing to do here: skip the descriptor scan and the render-target query both.
      if (rt0_blending)
         return false;
#endif

      // Only BOUND targets count: the wrapper leaves stale BlendEnable in unused slots, so the descriptor alone
      // matches nearly every single-target draw and would take over other hooks' passes.
      bool disagreement = false;
      for (UINT i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && !disagreement; i++)
         disagreement = bd.RenderTarget[i].BlendEnable != bd.RenderTarget[0].BlendEnable;
      if (!disagreement)
         return false;

      ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, nullptr);
      bool bound[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
         bound[i] = rtvs[i] != nullptr;
         if (rtvs[i])
            rtvs[i]->Release(); // OMGetRenderTargets hands back references; only the bound/not-bound answer is kept
      }

      // RT0's blend bit is loop-invariant, so the two shapes are mutually exclusive: one flag out of the loop.
      bool bound_disagreement = false;
      for (UINT i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && !bound_disagreement; i++)
         bound_disagreement = bound[i] && bd.RenderTarget[i].BlendEnable != bd.RenderTarget[0].BlendEnable;
      const bool needs_fix = bound_disagreement && !rt0_blending;
      [[maybe_unused]] const bool inverse_shape = bound_disagreement && rt0_blending;

#if DEVELOPMENT
      // One line per distinct shader, so a play session reports every pass that carries this.
      if (needs_fix || inverse_shape)
      {
         // Locked: this function deliberately runs on every context (see its header), so two can reach the set.
         static std::mutex logged_shaders_mutex;
         static std::unordered_set<uint64_t> logged_shaders;
         const uint64_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0];
         const std::scoped_lock logged_shaders_lock(logged_shaders_mutex);
         if (logged_shaders.emplace(pixel_shader_hash).second)
         {
            reshade::log::message(reshade::log::level::warning,
               std::format("[ME1-BlendFix] impossible per-RT blend state (dgVoodoo artefact) on pixel shader 0x{:X} - {}", pixel_shader_hash, needs_fix ? "repaired" : "inverse shape, left alone").c_str());
         }
      }
#endif

      if (!needs_fix)
         return false;

      ComPtr<ID3D11BlendState> fixed_state;
      if (const auto it = gd.fixed_blend_states.find(bd); it != gd.fixed_blend_states.end())
      {
         fixed_state = it->second;
      }
      else
      {
         D3D11_BLEND_DESC fixed_desc = bd;
         for (UINT i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
         {
            if (fixed_desc.RenderTarget[i].BlendEnable == bd.RenderTarget[0].BlendEnable)
               continue;
            const UINT8 write_mask = fixed_desc.RenderTarget[i].RenderTargetWriteMask; // legal per-RT in D3D9, keep it
            fixed_desc.RenderTarget[i] = bd.RenderTarget[0];
            fixed_desc.RenderTarget[i].RenderTargetWriteMask = write_mask;
         }
         if (FAILED(native_device->CreateBlendState(&fixed_desc, fixed_state.put())) || !fixed_state)
            return false; // leave the draw untouched rather than run it half-applied
         gd.fixed_blend_states[bd] = fixed_state;
      }

      native_device_context->OMSetBlendState(fixed_state.get(), blend_factor, sample_mask);
      (*original_draw_dispatch_func)();
      native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask); // hand the game back its own state
      return true;
   }

   // The resource behind the currently bound RTV 0, or null. Identifies the canvas at the final color pass, and
   // tests whether a later draw targets that same canvas. Not DEVELOPMENT-only: Hide UI needs it to ship.
   static ComPtr<ID3D11Resource> GetBoundRenderTargetResource(ID3D11DeviceContext* native_device_context)
   {
      ComPtr<ID3D11Resource> res;
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (rtv)
         rtv->GetResource(res.put());
      return res;
   }

#if DEVELOPMENT
   // The only honest read of an indirect format upgrade: the devkit sees the original handle and reports
   // r8g8b8a8_typeless / isResourceUpgraded:false either way (BL2). Core rebinds substituted RTVs first.
   static void DumpBoundRenderTarget(ID3D11DeviceContext* native_device_context, const char* label)
   {
      char msg[256];
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (!rtv)
      {
         std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: %s RTV0 NOT BOUND", label);
         reshade::log::message(reshade::log::level::info, msg);
         return;
      }
      uint4 size;
      DXGI_FORMAT format;
      GetResourceInfo(rtv.get(), size, format);
      std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: %s RTV0 %ux%u res_fmt %s -> upgrade %s",
         label, size.x, size.y, GetFormatNameSafe(format),
         format == DXGI_FORMAT_R16G16B16A16_FLOAT ? "OK (fp16)" : "MISSING (not fp16)");
      reshade::log::message(reshade::log::level::info, msg);
   }

   // One-shot synchronous readback of dgVoodoo's PS constant mirror (b4): the devkit shows the buffers but not their
   // contents, and which pass holds the real display gamma decides the whole encode contract. Stalls once per pass.
   static void DumpConstantRows(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const char* label, uint32_t first_row, uint32_t row_count, bool vertex_stage = false)
   {
      ComPtr<ID3D11Buffer> cb;
      if (vertex_stage)
         native_device_context->VSGetConstantBuffers(4, 1, cb.put());
      else
         native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      if (!cb)
         return;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (first_row + row_count) * 16)
         return;
      D3D11_BUFFER_DESC sd = {};
      sd.ByteWidth = bd.ByteWidth;
      sd.Usage = D3D11_USAGE_STAGING;
      sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      ComPtr<ID3D11Buffer> staging;
      if (FAILED(native_device->CreateBuffer(&sd, nullptr, staging.put())))
         return;
      native_device_context->CopyResource(staging.get(), cb.get());
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)) || mapped.pData == nullptr)
         return;
      const float* rows = (const float*)mapped.pData;
      for (uint32_t r = first_row; r < first_row + row_count; r++)
      {
         char msg[256];
         std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: %s cb4[%u] = %.6f %.6f %.6f %.6f", label, r, rows[r * 4 + 0], rows[r * 4 + 1], rows[r * 4 + 2], rows[r * 4 + 3]);
         reshade::log::message(reshade::log::level::info, msg);
      }
      native_device_context->Unmap(staging.get(), 0);
   }

#if ENABLE_SMAA
   // Calibration readback for g_smaa_pred_tolerance (MoHA method). Coverage above SMAA_PREDICATION_THRESHOLD (0.5) is
   // the fraction of the frame predication treats as geometry: real silhouettes are ~1% of a typical frame; tens of
   // percent = tolerance too low (mask saturated = no predication at all), ~0 = too high (silhouettes never regain
   // sensitivity). A working mask is BIMODAL: the fraction barely moves across a 20x tolerance sweep. 1024 buckets so
   // the low percentiles resolve (64 pinned p50 to the first bucket on MoHA). One-shot: the Map stalls the GPU.
   static void MeasurePredicationMask(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Texture2D* pred)
   {
      // Desc taken off the mask, not rebuilt: CopyResource requires the two to agree, and the walk below has to cover
      // exactly what was copied.
      D3D11_TEXTURE2D_DESC td;
      pred->GetDesc(&td);
      td.Usage = D3D11_USAGE_STAGING;
      td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      td.BindFlags = 0;
      td.MiscFlags = 0;
      ComPtr<ID3D11Texture2D> staging;
      if (FAILED(native_device->CreateTexture2D(&td, nullptr, staging.put())))
         return;
      native_device_context->CopyResource(staging.get(), pred);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)) || mapped.pData == nullptr)
         return;

      constexpr int kBuckets = 1024;
      uint64_t histogram[kBuckets] = {};
      uint64_t total = 0, above_half = 0, nans = 0;
      double sum = 0.0;
      for (uint32_t y = 0; y < td.Height; y++)
      {
         const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + (size_t)y * mapped.RowPitch);
         for (uint32_t x = 0; x < td.Width; x++)
         {
            const float v = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
            if (std::isnan(v))
            {
               nans++; // the CS met inf - inf in the scene alpha: counted, kept out of the mean
               continue;
            }
            total++;
            sum += v;
            if (v > 0.5f)
               above_half++;
            histogram[std::clamp((int)(v * kBuckets), 0, kBuckets - 1)]++;
         }
      }
      native_device_context->Unmap(staging.get(), 0);
      if (total == 0)
         return;

      auto percentile = [&](double fraction)
      {
         const uint64_t target = (uint64_t)(fraction * (double)total);
         uint64_t running = 0;
         for (int i = 0; i < kBuckets; i++)
         {
            running += histogram[i];
            if (running >= target)
               return (float)(i + 1) / (float)kBuckets;
         }
         return 1.f;
      };
      char msg[512];
      std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: SMAA predication mask %ux%u tolerance %.4f -> above 0.5 = %.2f%% | mean %.4f | p50 %.4f p90 %.4f p99 %.4f | NaN %llu",
         td.Width, td.Height, g_smaa_pred_tolerance, 100.0 * (double)above_half / (double)total, sum / (double)total, percentile(0.50), percentile(0.90), percentile(0.99), (unsigned long long)nans);
      reshade::log::message(reshade::log::level::info, msg);
   }
#endif // ENABLE_SMAA
#endif // DEVELOPMENT

public:
   void OnInit(bool async) override
   {
      // Game-specific toggles consumed by the replaced pass (Luma_ME1_Tonemap.hlsl).
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: recover highlights + DICE display map"},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

#if ENABLE_SMAA
      // The 6 SMAA passes are auto-registered by core from Luma_SMAA_impl. Only the predication CS is ours: it
      // turns the scene buffer's alpha (linear depth) into an R16F edge-ness signal in [0,1].
      native_shaders_definitions.emplace(CompileTimeStringHash("ME1 Depth Extract CS"),
         ShaderDefinition("Luma_ME1_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // RCAS sharpen PS, drawn via core "Copy VS" + DrawCustomPixelShader after SMAA.
      native_shaders_definitions.emplace(CompileTimeStringHash("ME1 Sharpen PS"),
         ShaderDefinition{"Luma_ME1_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
#endif

      // Address-space ceiling, PROBED rather than assumed. A 32-bit process gets 2 GB unless the exe carries
      // LARGE_ADDRESS_AWARE; stock MassEffect.exe does not (0x0102 measured on an untouched copy, 0x0122 after a
      // patcher). lpMaximumApplicationAddress is the one-call answer and is what core's own address-space math reads
      // (Source/Core/utils/system.cpp). Warned, not silently degraded: Luma's 4K scratch is ~365 MB (SMAA snapshot 66 +
      // predication 17 + core SMAA ~83 + RCAS 66 + Karis 66 + pyramid 66) on top of dgVoodoo's D3D11 copies and the
      // fp16 format mirrors, so on 2 GB the bad_alloc the 4K packs already produced (NOTES.md) arrives much sooner -
      // but which of resolution, packs, SMAA or bloom to give up is the user's call, and 2 GB is playable without packs.
      static bool address_space_checked = false;
      if (!address_space_checked)
      {
         address_space_checked = true;
         SYSTEM_INFO sys_info = {};
         GetSystemInfo(&sys_info);
         if (reinterpret_cast<uintptr_t>(sys_info.lpMaximumApplicationAddress) < 0x80000000u)
            reshade::log::message(reshade::log::level::warning,
               "[Luma] ME1: this executable is NOT large-address-aware, so the process is capped at 2 GB of address "
               "space. Luma's own scratch is ~365 MB at 4K, on top of dgVoodoo's copies: expect std::bad_alloc (UE3 "
               "reports it as a rendering GPF) with 4K texture packs, with the devkit, or both. Patch the "
               "LARGE_ADDRESS_AWARE bit, or lower the resolution / turn off SMAA and HDR bloom.");
      }

      // Buffers stay in GAMMA space: the gamma-SDR HUD blends onto this canvas and a linear buffer washes it out.
      // Replaced passes pre-scale by Game/UIPaperWhite (UI_DRAW_TYPE 2); core composition encodes scRGB.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // game shipped gamma-2.2 SDR
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1'); // gamut-map wild colors in composition
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');       // HUD gets its own UIPaperWhite + gamma blend

      // dgVoodoo binds b0-b5 only (measured on every captured draw), so b12/b13 are free for Luma.
      // luma_data is used by the Display Composition; luma_ui stays off (UI drawn by the game).
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level. Core gates the separate
      // "UI Paper White" slider on UI_DRAW_TYPE >= 1 && !use_os_reference_white_level.
      use_os_reference_white_level = false;

      // User grade controls (read in Luma_ME1_Tonemap.hlsl via LumaSettings.GameSettings). All vanilla by default.
      default_luma_global_game_settings.Exposure = 1.f; // multiplier (1x)
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f; // off by default; only the mandatory DICE/gamut desaturation applies
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Dithering = 1.f; // subtle anti-banding on by default
      default_luma_global_game_settings.LumaBloomEnable = ENABLE_BLOOM ? 1.f : 0.f;
      // 1.0 is exactly where the game's own bright-pass sits (DofBloomGather_0x56854256: any channel > 1.0).
      default_luma_global_game_settings.BloomThreshold = 1.f;
      // Light AutoHDR on the Bink movie pass (Video_0x1A82565B): movies bypass the scene passes entirely, so
      // without it they sit flat at paper white. The pair is BL2's calibrated one (peak ~165 nits at 0.5).
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      // Until the first readback lands: the UE3 default DisplayGamma 2.2. Measured live value here is 0.625 (1/1.6).
      default_luma_global_game_settings.DisplayGammaInverse = 1.f / 2.2f;
      // Until the gather reports: the value measured on the Citadel.
      default_luma_global_game_settings.BloomScaleLive = 0.1f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MassEffectGameDeviceData;
   }

   // Core calls this at device destruction but never frees "device_data.game", so the allocation is ours (TW2/BL2).
   // GameDeviceData has no virtual destructor: delete through the concrete type or members leak.
   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      delete static_cast<MassEffectGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   // Per-draw data (b12), uploaded at the gamma-correction seam: whether the uber pass ran earlier this frame. The
   // engine skips UberPostProcessBlend in elevators and some loading/UI scenes (measured: a gamma-only frame with
   // Overlay.w = 1), and the gamma-correction replacement then reads the RAW fp16 scene, so it has to know.
   void UpdateLumaInstanceDataCB(CB::LumaInstanceDataPadded& data, CommandListData& cmd_list_data, DeviceData& device_data) override
   {
      data.GameData.UberRanThisFrame = GetGameDeviceData(device_data).has_drawn_tonemap ? 1.f : 0.f;
   }

#if ENABLE_BLOOM
   // Core's DrawKarisAverage output: full-res fp16, ~66 MB at 4K (it inherits the scene texture's size). Core
   // drops only the UAV, and only on swapchain init, so a feature-off toggle has to release both views itself.
   // Guarded with BLOOM, not SMAA: the Karis average is a bloom resource and its only caller is the bloom-off
   // release in OnPresent, so pairing it with SMAA made ENABLE_SMAA 0 + ENABLE_BLOOM 1 fail to compile.
   static void ReleaseCoreKarisAverage(DeviceData& device_data)
   {
      auto& mr = device_data.managed_resources;
      mr.unordered_access_views[CompileTimeStringHash("luma_karis_average")].reset();
      mr.shader_resource_views[CompileTimeStringHash("luma_karis_average")].reset();
   }
#endif

#if ENABLE_SMAA
   // Core's DrawSMAA intermediates, ~83 MB at 4K, sized from the RTV handed to them and dropped only on swapchain
   // init, not this canvas' resize. The SRVs hold their own reference, so release both.
   static void ReleaseCoreSMAAIntermediates(DeviceData& device_data)
   {
      auto& mr = device_data.managed_resources;
      mr.depth_stencil_views[CompileTimeStringHash("smaa_dsv")].reset();
      mr.render_target_views[CompileTimeStringHash("smaa_edge_detection")].reset();
      mr.render_target_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
      mr.shader_resource_views[CompileTimeStringHash("smaa_edge_detection")].reset();
      mr.shader_resource_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
   }

   // SMAA on the graded gamma canvas from the post-draw callback, so it lands after the grade and before the HUD.
   // TW2/BL2 chain: snapshot -> SRV -> DrawSMAA, last pass into the canvas RTV. No-op if incomplete.
   void RunPostFinalGradeSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffectGameDeviceData& gd, ID3D11Resource* canvas_res, ID3D11RenderTargetView* canvas_rtv)
   {
      uint4 cinfo{};
      DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(canvas_res, cinfo, cfmt);
      const uint32_t w = cinfo.x, h = cinfo.y;
      if (w == 0 || h == 0 || cfmt == DXGI_FORMAT_UNKNOWN)
         return;

      // Shader-readiness gate (async loader / dev live-reload): skip SMAA this frame if anything is missing.
      const bool smaa_ready =
         AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")}) && AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")});
      if (!smaa_ready)
         return;

      // Drop DrawSMAA's core-managed intermediates on resolution change so they recreate at the new size.
      if (gd.smaa_core_w != w || gd.smaa_core_h != h)
      {
         ReleaseCoreSMAAIntermediates(device_data);
         gd.smaa_core_w = w;
         gd.smaa_core_h = h;
      }

      // Edge-ness from the scene alpha (linear depth). Scale and mask fall back together: 2.0 with a null mask would
      // raise the threshold frame-wide. The CS maps texels 1:1, hence the size check.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME1 Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd.srv_scene.get() != nullptr && pred_cs != nullptr;
      if (pred_ok)
      {
         uint4 sinfo{};
         DXGI_FORMAT sfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(gd.srv_scene.get(), sinfo, sfmt);
         pred_ok = sinfo.x == w && sinfo.y == h;
      }
      if (pred_ok)
      {
         if (!gd.cb_pred || gd.pred_tolerance != g_smaa_pred_tolerance)
         {
            const float p[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, p, sizeof(p), gd.cb_pred))
               gd.pred_tolerance = g_smaa_pred_tolerance;
         }
         if (!gd.tex_pred || gd.pred_w != w || gd.pred_h != h)
         {
            gd.ReleasePredicationScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_pred, DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, gd.uav_pred.put());
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, gd.srv_pred.put());
               gd.pred_w = w;
               gd.pred_h = h;
            }
         }
         pred_ok = gd.cb_pred && gd.uav_pred && gd.srv_pred;
      }

      const float pred_scale = pred_ok ? 2.f : 1.f;
      if (!gd.cb_smaa_metrics || gd.smaa_metrics_w != w || gd.smaa_metrics_h != h || gd.smaa_metrics_pred_scale != pred_scale)
      {
         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
         if (CreateImmutableCB(native_device, metrics, sizeof(metrics), gd.cb_smaa_metrics))
         {
            gd.smaa_metrics_w = w;
            gd.smaa_metrics_h = h;
            gd.smaa_metrics_pred_scale = pred_scale;
         }
      }
      if (!gd.cb_smaa_metrics)
         return;

      // RCAS decides the chain's SHAPE, so resolve it before allocating anything: with sharpening off the last
      // SMAA pass writes the canvas directly, which removes both a full-frame write-back and the intermediate.
      // Core's fullscreen "Copy VS", shared by RCAS below and by the predication debug view.
      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME1 Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && copy_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen)
      {
         if (!gd.cb_sharpen || gd.sharpen_w != w || gd.sharpen_h != h || gd.sharpen_amount != g_rcas_sharpness)
         {
            const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
            if (CreateImmutableCB(native_device, sp, sizeof(sp), gd.cb_sharpen))
            {
               gd.sharpen_w = w;
               gd.sharpen_h = h;
               gd.sharpen_amount = g_rcas_sharpness;
            }
         }
         if (!gd.tex_smaa_out || gd.smaa_out_w != w || gd.smaa_out_h != h)
         {
            gd.ReleaseSharpenScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.tex_smaa_out, cfmt))
            {
               native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
               gd.smaa_out_w = w;
               gd.smaa_out_h = h;
            }
         }
         if (!gd.cb_sharpen || !gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
            do_sharpen = false; // allocation failed: fall back to the un-sharpened chain rather than dropping SMAA
      }

      if (!gd.tex_input || gd.smaa_temps_w != w || gd.smaa_temps_h != h)
      {
         gd.srv_input.reset();
         gd.tex_input.reset();
         if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input, cfmt))
         {
            native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
            gd.smaa_temps_w = w;
            gd.smaa_temps_h = h;
         }
      }
      if (!gd.srv_input)
         return;

      native_device_context->CopyResource(gd.tex_input.get(), canvas_res);

      // Scene alpha (linear depth) -> plane-deviation edge-ness in R16F; see Luma_ME1_DepthExtract.hlsl for why
      // this is an edge test rather than a depth rescale.
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> pred_cs_state;
         pred_cs_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11ShaderResourceView* ps_srv = gd.srv_scene.get();
         ID3D11UnorderedAccessView* ps_uav = gd.uav_pred.get();
         ID3D11Buffer* ps_cb = gd.cb_pred.get();
         native_device_context->CSSetShaderResources(0, 1, &ps_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &ps_uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &ps_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

         pred_cs_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      // Calibration aids, driven from the Anti-Aliasing section. Both read the mask that was just written.
      if (g_smaa_pred_measure)
      {
         g_smaa_pred_measure = false;
         if (pred_ok)
            MeasurePredicationMask(native_device, native_device_context, gd.tex_pred.get());
         else
            reshade::log::message(reshade::log::level::warning, "[Luma] ME1 DIAG: SMAA predication mask not measured: predication inactive this frame (no scene capture, size mismatch or CS missing)");
      }
      if (pred_ok && g_smaa_pred_debug)
      {
         auto* copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs != nullptr && copy_ps != nullptr)
         {
            // The mask is single-channel, so the core copy lands it in RED — unmistakably a debug view. Replaces
            // the antialiased frame rather than blending over it, hence the early return.
            DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
            debug_state.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               copy_vs, copy_ps, gd.srv_pred.get(), canvas_rtv, w, h, false);
            debug_state.Restore(native_device_context);
            return;
         }
      }
#endif

      // Metrics CB at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs, but not cbuffers).
      ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
      native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
      native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
      ID3D11Buffer* mcb = gd.cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &mcb);
      native_device_context->PSSetConstantBuffers(1, 1, &mcb);

      // Reading the canvas as the target is safe: the chain samples the snapshot, never the canvas itself.
      DrawSMAA(native_device, native_device_context, device_data, do_sharpen ? gd.tex_smaa_out_rtv.get() : canvas_rtv, gd.srv_input.get(), gd.srv_input.get(), pred_ok ? gd.srv_pred.get() : nullptr /*predication signal*/);

      // RCAS on the SMAA output, written into the canvas.
      if (do_sharpen)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11Buffer* scb = gd.cb_sharpen.get();
         native_device_context->PSSetConstantBuffers(0, 1, &scb);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, sharpen_ps, gd.tex_smaa_out_srv.get(), canvas_rtv, w, h, false);

         sharpen_state.Restore(native_device_context);
      }

      ID3D11Buffer* vcb = vs_cb1_orig.get();
      ID3D11Buffer* pcb = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vcb);
      native_device_context->PSSetConstantBuffers(1, 1, &pcb);
   }
#endif // ENABLE_SMAA

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

      // Hide HUD: cancel draws that run AFTER the final color pass AND target the same canvas. The render-target test
      // is the load-bearing one - "everything after the tonemap" also swallows dgVoodoo's present blit. The custom-pass
      // clause deliberately does NOT exclude hash-replaced HUD draws: core reports is_custom_pass for those too (it is
      // "cloned VS or PS"), and excluding them left the HUD on screen. It only rejects a draw with a cloned shader and
      // no game pixel-shader hash at all, i.e. a pass Luma injected rather than replaced.
      if (g_hide_ui && is_immediate && (!is_custom_pass || original_shader_hashes.pixel_shaders[0] != UINT64_MAX) && gd.has_drawn_final && gd.canvas_res)
      {
         ComPtr<ID3D11Resource> rt = GetBoundRenderTargetResource(native_device_context);
         if (rt.get() == gd.canvas_res.get())
            return DrawOrDispatchOverrideType::Replaced;
      }

#if ENABLE_BLOOM
      // The gather runs before the uber: read its BloomScale here (deferred, no stall). Bloom-only work, so it follows
      // the feature's own switch - GameSettings.BloomScaleLive is read by nothing else (Luma_ME1_Tonemap.hlsl LumaBloom).
      if (g_luma_bloom_enable && is_immediate && !gd.bloom_cb_ring.advanced_this_frame && IsDofBloomGather(original_shader_hashes))
         TrackCB4Row(native_device, native_device_context, gd.bloom_cb_ring, 11, 0.f, 4.f, &gd.bloom_scale_live, &gd.bloom_scale_valid); // engine BloomScale, measured 0.1
#endif

#if DEVELOPMENT
      // HUD permutation net (see diag_post_final_ps). A dump sweep for the float4 cxform (`mad c.xyzw, c.xyzw,
      // cb4[10].xyzw, cb4[11].xyzw` into o0) returns exactly the five replaced families x both wrapper builds, so the
      // family is complete FOR WHAT HAS BEEN RENDERED - a screen nobody visited never compiled its shader and cannot
      // be in the dump at all, which is what this net is still here for (NOTES.md keeps the screen list). Logs the
      // first sighting of each pixel shader that draws onto the canvas after the gamma-correction pass, with the GFx
      // colour transform rows (10 = mul, 11 = add) and the blend state.
      if (is_immediate && !is_custom_pass && gd.has_drawn_final && gd.canvas_res && original_shader_hashes.pixel_shaders[0] != UINT64_MAX)
      {
         const uint32_t ps_hash = (uint32_t)original_shader_hashes.pixel_shaders[0];
         if (!gd.diag_post_final_ps.contains(ps_hash) && GetBoundRenderTargetResource(native_device_context).get() == gd.canvas_res.get())
         {
            gd.diag_post_final_ps.insert(ps_hash);
            ComPtr<ID3D11BlendState> blend_state;
            float blend_factor[4];
            UINT sample_mask;
            native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
            D3D11_BLEND_DESC blend_desc = {};
            if (blend_state)
               blend_state->GetDesc(&blend_desc);
            const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
            char label[128];
            std::snprintf(label, sizeof(label), "post-final canvas PS 0x%08X UNREPLACED blend %u src %u dst %u srcA %u dstA %u", ps_hash, rt0.BlendEnable, rt0.SrcBlend, rt0.DestBlend, rt0.SrcBlendAlpha, rt0.DestBlendAlpha);
            DumpConstantRows(native_device, native_device_context, label, 10, 2);
         }
      }

      // Producer-side gate for the bloom buffer upgrade: the devkit cannot see an indirect upgrade at all, so the
      // render target bound at this draw is the only place the fp16 mirror is observable.
      if (is_immediate && !gd.diag_logged_gather && IsDofBloomGather(original_shader_hashes))
      {
         gd.diag_logged_gather = true;
         DumpBoundRenderTarget(native_device_context, "bloom buffer");
         // Vanilla bloom model: BloomScale (PS row 11.x) and the 16 tap offsets (VS rows 20..27, xy/wz pairs in UV units).
         DumpConstantRows(native_device, native_device_context, "gather PS", 8, 4);
         DumpConstantRows(native_device, native_device_context, "gather VS", 20, 8, true);
      }
      // The separable blur that follows: 9 weights (PS rows 10..18, float4 each) and 4 offset pairs (VS rows 25..29),
      // for both directions of one frame.
      if (is_immediate && gd.diag_filter_dumps < 2 && ContainsPixelShader(original_shader_hashes, kBloomFilterHash, kBloomFilterHash_v281))
      {
         gd.diag_filter_dumps++;
         DumpConstantRows(native_device, native_device_context, gd.diag_filter_dumps == 1 ? "filter#1 PS" : "filter#2 PS", 10, 9);
         DumpConstantRows(native_device, native_device_context, gd.diag_filter_dumps == 1 ? "filter#1 VS" : "filter#2 VS", 25, 5, true);
      }
#endif

      // Wrapper-build telemetry, deliberately AHEAD of the gate below: an unkeyed dgVoodoo build has to be
      // reported whichever context recorded the pass (the warning itself fires in OnPresent).
      if (!gd.ever_matched_final_pass && (IsFinalColorPass(original_shader_hashes) || IsUberPostPass(original_shader_hashes)))
         gd.ever_matched_final_pass = true;

#if DEVELOPMENT
      // Which pass gamma-encodes: the uber's GammaColorScaleAndInverse.w (row 15), the copy's exponent (row 8) or the
      // gamma-correction's inverse gamma (row 11). Measured: 1.0 / 1.0 / 0.625. Re-dump from the Grade section's
      // button, on a gameplay frame: the first frame is a loading fade with a degenerate grade.
      if (is_immediate && g_dump_pass_cb && ContainsPixelShader(original_shader_hashes, kCopyPassHash, kCopyPassHash_v281))
         DumpConstantRows(native_device, native_device_context, "copy 0x1E37D75B", 8, 1);
#endif

      // The uber pass: HDR grade into the fp16 intermediate. Read the hash list before any early-out: is_custom_pass is
      // true for hash-replaced passes too. Gated on is_immediate (BL GOTY does the same).
      if (is_immediate && !gd.has_drawn_tonemap && IsUberPostPass(original_shader_hashes))
      {
         gd.has_drawn_tonemap = true;

         // Push LumaSettings HERE, at the seam, not inside a feature block: every consumer below reads b13 (the
         // bloom prefilter's BloomThreshold, and the grade itself). With this inside the bloom block, turning Luma
         // bloom off left the grade reading the previous upload (MoHA). Above the state stack too: Restore() rolls PS
         // constant buffers back wholesale. "updated_cbuffers" is left alone so core still uploads afterwards.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);

         // The scene is bound at t0 right here (scene A, fp16, alpha = linear depth): the bloom source and the SMAA
         // predication source. No separate capture pass to keep in sync with this one.
         gd.srv_scene.reset();
         native_device_context->PSGetShaderResources(0, 1, gd.srv_scene.put());

#if DEVELOPMENT
         if (g_dump_pass_cb)
         {
            DumpBoundRenderTarget(native_device_context, "uber target (scene B)");
            DumpConstantRows(native_device, native_device_context, "uber 0xAC8341E0", 8, 9);
         }
#endif

#if ENABLE_BLOOM
         // Luma bloom pyramid off the fp16 LINEAR scene at t0, pre-glow by construction (the halo is added later, in
         // the grade). Karis average first: no TAA, so fireflies die spatially.
         gd.srv_luma_bloom.reset(); // DrawBloom AddRef's its mip 0 into this
         if (g_luma_bloom_enable && gd.srv_scene)
         {
            DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
            bloom_state.Cache(native_device_context, device_data.uav_max_count);

            ComPtr<ID3D11ShaderResourceView> srv_karis;
            DrawKarisAverage(native_device, native_device_context, device_data, gd.srv_scene.get(), srv_karis.put());
            // The sigmas are in mip texels, so nothing here is resolution-dependent.
            if (srv_karis)
               DrawBloom(native_device, native_device_context, device_data, srv_karis.get(), (int)std::size(g_bloom_sigmas), g_bloom_sigmas, gd.srv_luma_bloom.put());

            bloom_state.Restore(native_device_context);
         }
         {
            // Bound every frame, null included: the composite is gated on LumaBloomEnable, not on the slot, and
            // dgVoodoo's placeholder would be sampled as garbage. The composite ADDS this.
            ID3D11ShaderResourceView* bloom_srv = gd.srv_luma_bloom.get();
            native_device_context->PSSetShaderResources(kLumaBloomSlot, 1, &bloom_srv);
         }
#endif
      }

      // The gamma-correction pass ends main post processing: it reads the intermediate and writes the 8-bit canvas.
      if (is_immediate && !gd.has_drawn_final && IsFinalColorPass(original_shader_hashes))
      {
         gd.has_drawn_final = true;
         device_data.has_drawn_main_post_processing = true;

         // Same seam rule as the uber pass: this replacement reads LumaSettings (paper white, dither) and LumaData (the
         // UberRanThisFrame flag, see UpdateLumaInstanceDataCB), and the SMAA path below returns Replaced, which skips
         // core's own upload for this draw.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData);

         // Gamma-only frames (elevators, some loading/UI scenes; UberRanThisFrame = 0): the uber never ran, so nothing
         // captured the scene for SMAA predication. This pass then reads the RAW fp16 scene at t0 (alpha = linear
         // depth), so capture it here rather than drop predication for the whole frame. The size gate still applies.
         if (!gd.has_drawn_tonemap)
         {
            gd.srv_scene.reset();
            native_device_context->PSGetShaderResources(0, 1, gd.srv_scene.put());
         }

         // Remember what this pass draws into: Hide UI needs the resource identity, SMAA needs the view. Captured
         // every frame because an indirect upgrade or a resolution change can swap the mirror.
         ComPtr<ID3D11RenderTargetView> canvas_rtv;
         native_device_context->OMGetRenderTargets(1, canvas_rtv.put(), nullptr);
         gd.canvas_res.reset();
         if (canvas_rtv)
            canvas_rtv->GetResource(gd.canvas_res.put());

#if DEVELOPMENT
         // Primary gate for the canvas upgrade: with indirect upgrades the devkit only ever sees the original
         // r8g8b8a8 resource, so the bound render target here is the only honest read of the fp16 mirror.
         if (!gd.diag_logged_rt)
         {
            gd.diag_logged_rt = true;
            DumpBoundRenderTarget(native_device_context, "canvas");
         }
         if (g_dump_pass_cb)
         {
            DumpConstantRows(native_device, native_device_context, "gamma 0x17CE0932", 8, 4);
            g_dump_pass_cb = false; // last of the frame's three dumps (copy -> uber -> gamma), so disarm here
         }
#endif

         // The display gamma this pass applies, for the grade's SDR reference (see the device data). Already once per
         // frame: this whole block is gated on !has_drawn_final, which it sets above.
         TrackCB4Row(native_device, native_device_context, gd.gamma_cb_ring, 11, 0.25f, 1.f, &gd.gamma_inverse_live, &gd.gamma_inverse_valid); // 1/gamma for gamma in [1, 4]

#if ENABLE_SMAA
         // Run the pass ourselves, then SMAA on the canvas, so the antialiasing lands before the HUD. Falls back to
         // a plain draw when the callback is unavailable (one frame without AA) rather than skipping the encode.
         if (g_smaa_enable && original_draw_dispatch_func != nullptr && canvas_rtv && gd.canvas_res)
         {
            (*original_draw_dispatch_func)();
            RunPostFinalGradeSMAA(native_device, native_device_context, device_data, gd, gd.canvas_res.get(), canvas_rtv.get());
            return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
         }
#endif
      }

      // Blend repair LAST on purpose: it re-issues the draw, so it must yield to every hook above or it runs vanilla
      // a pass another hook meant to take over (it disabled a hook in TW2).
      if (FixImpossiblePerRTBlend(native_device, native_device_context, gd, stages, original_shader_hashes, is_custom_pass, original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      return DrawOrDispatchOverrideType::None; // never cancel the original draw (the replacement is by hash)
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // One-shot telemetry (BL GOTY precedent). The frame budget is warmup only: the menu runs the same pass, so
      // a few frames are enough.
      constexpr uint32_t kBuildCheckFrame = 120;
      if (!gd.build_check_done && ++gd.frames_presented >= kBuildCheckFrame)
      {
         gd.build_check_done = true;
         if (!gd.ever_matched_final_pass)
            reshade::log::message(reshade::log::level::warning,
               "[Luma] ME1: no keyed final color pass seen after warmup -- the dgVoodoo build is probably neither 2.87.3 nor 2.81.3, so every shader replacement is inactive (re-dump the shaders for it).");
      }

      gd.has_drawn_tonemap = false;
      gd.has_drawn_final = false;
      gd.gamma_cb_ring.advanced_this_frame = false; // re-arm the once-per-frame ring advances
      gd.bloom_cb_ring.advanced_this_frame = false;

      // Publish the gamma pass's inverse display gamma to the grade (GameSettings is the only channel between passes).
      if (gd.gamma_inverse_valid && std::abs(cb_luma_global_settings.GameSettings.DisplayGammaInverse - gd.gamma_inverse_live) > 1e-4f)
      {
         cb_luma_global_settings.GameSettings.DisplayGammaInverse = gd.gamma_inverse_live;
         device_data.cb_luma_global_settings_dirty = true;
      }
#if ENABLE_BLOOM
      // Same for the gather's BloomScale, the gain that makes Bloom Intensity 1 vanilla strength.
      if (gd.bloom_scale_valid && std::abs(cb_luma_global_settings.GameSettings.BloomScaleLive - gd.bloom_scale_live) > 1e-5f)
      {
         cb_luma_global_settings.GameSettings.BloomScaleLive = gd.bloom_scale_live;
         device_data.cb_luma_global_settings_dirty = true;
      }
#endif

      gd.canvas_res.reset(); // do not hold a reference across frames: it would outlive a resize or a mirror swap
      // Core never clears this, so leaving it set would claim a tonemapped scene on frames with no final pass
      // (movies, loading). Inert here: consumers need enable_ui_separation (off).
      device_data.has_drawn_main_post_processing = false;
      gd.srv_scene.reset(); // recaptured every frame at the uber pass (gamma-only frames: at the gamma pass); never hold it across one

#if ENABLE_BLOOM
      // Give the address space back when the pyramid is off, on the render thread. Unconditional while off: resetting
      // empty entries is two map lookups. Only core's Karis buffer is reachable.
      if (!g_luma_bloom_enable)
         ReleaseCoreKarisAverage(device_data);
#endif

#if ENABLE_SMAA
      // Give the address space back when a feature is off. Done here rather than in the ImGui handler so the
      // release happens on the render thread, never while a frame is mid-flight.
      if (!g_smaa_enable && gd.tex_input)
      {
         gd.ReleaseSMAAScratch();
         ReleaseCoreSMAAIntermediates(device_data);
         gd.smaa_core_w = gd.smaa_core_h = 0; // core recreates lazily; keep our latch from claiming they are current
      }
      else
      {
         if (!g_smaa_predication && gd.tex_pred)
            gd.ReleasePredicationScratch();
         if (g_rcas_sharpness <= 0.f && gd.tex_smaa_out)
            gd.ReleaseSharpenScratch();
      }
#endif
   }

   void LoadConfigs() override
   {
      // Grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on first frame).
      reshade::get_config_value(nullptr, NAME, "Exposure", cb_luma_global_settings.GameSettings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", cb_luma_global_settings.GameSettings.HighlightDechroma);
#if ENABLE_BLOOM
      // Inside the guard because it drives the Luma pyramid alone: with no pyramid there is nothing for it to scale.
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", cb_luma_global_settings.GameSettings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      cb_luma_global_settings.GameSettings.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f; // mirror to both shaders
      reshade::get_config_value(nullptr, NAME, "BloomThreshold", cb_luma_global_settings.GameSettings.BloomThreshold);
#endif
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
#if ENABLE_SMAA
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
#if ENABLE_SMAA
      ImGui::SeparatorText("Anti-Aliasing");
      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds SMAA anti-aliasing (the game has none of its own).");
      if (g_smaa_enable)
      {
#if DEVELOPMENT
         // Predication is not a preference: it only relaxes the edge threshold back to base ULTRA on geometry and
         // never below, so off is strictly worse. Kept as a bisect switch for devs, shipped on and out of sight.
         if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
            reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.");
         if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
            reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
         ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
         if (ImGui::Button("Measure Predication Mask"))
            g_smaa_pred_measure = true;
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log the mask's coverage above 0.5 plus percentiles to ReShade.log.\nReal silhouettes are ~1% of a typical frame; a working mask barely moves across a 20x tolerance sweep.\nStalls the GPU for one frame.");
#endif

         if (ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f))
            reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sharpening applied on top of SMAA (0 = off).");
         DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness"); // writes the config itself (Serialize defaults true)
      }
#endif

      // --- HDR grade (read in Luma_ME1_Tonemap.hlsl via LumaSettings.GameSettings; HDR tonemap path only) ---
      auto& gs = cb_luma_global_settings.GameSettings;
      ImGui::SeparatorText("Grade");

      if (ImGui::SliderFloat("Exposure", &gs.Exposure, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "Exposure", gs.Exposure);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Overall image brightness (1 = vanilla).");
      if (DrawResetButton(gs.Exposure, default_luma_global_game_settings.Exposure, "Exposure"))
         device_data.cb_luma_global_settings_dirty = true;

      if (ImGui::SliderFloat("Contrast", &gs.Contrast, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "Contrast", gs.Contrast);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Overall image contrast, HDR only (1 = vanilla).");
      if (DrawResetButton(gs.Contrast, default_luma_global_game_settings.Contrast, "Contrast"))
         device_data.cb_luma_global_settings_dirty = true;

      if (ImGui::SliderFloat("Saturation", &gs.Saturation, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "Saturation", gs.Saturation);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Color saturation, HDR only (1 = vanilla).");
      if (DrawResetButton(gs.Saturation, default_luma_global_game_settings.Saturation, "Saturation"))
         device_data.cb_luma_global_settings_dirty = true;

      if (ImGui::SliderFloat("Highlights Desaturation", &gs.HighlightDechroma, 0.f, 1.f))
      {
         reshade::set_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightDechroma);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("How soon bright sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      if (DrawResetButton(gs.HighlightDechroma, default_luma_global_game_settings.HighlightDechroma, "HighlightsDesaturation"))
         device_data.cb_luma_global_settings_dirty = true;

#if DEVELOPMENT
      if (ImGui::Button("Dump Pass Constants"))
         g_dump_pass_cb = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Log the copy/uber/gamma passes' cb4 grade rows for the next frame to ReShade.log.\nMeasured 1.0 / 1.0 / 0.625 - press it on a gameplay frame, not on a loading fade.\nEach dump blocks on a GPU read.");
#endif

#if ENABLE_BLOOM
      ImGui::SeparatorText("Bloom");
      if (ImGui::Checkbox("Luma Bloom Enable", &g_luma_bloom_enable))
      {
         reshade::set_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
         gs.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's bloom with a wider, softer HDR bloom.");

      // Everything below drives the Luma pyramid and nothing else, so it all greys out with it: no slider here
      // can reach the game's own glow.
      ImGui::BeginDisabled(!g_luma_bloom_enable);

      if (ImGui::SliderFloat("Bloom Intensity", &gs.BloomIntensity, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Bloom strength (1 = vanilla, 0 = none).");
      if (DrawResetButton(gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, "BloomIntensity"))
         device_data.cb_luma_global_settings_dirty = true;

#if DEVELOPMENT
      if (ImGui::SliderFloat("Bloom Threshold", &gs.BloomThreshold, 0.f, 4.f, "%.2f"))
      {
         reshade::set_config_value(nullptr, NAME, "BloomThreshold", gs.BloomThreshold);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Linear scene brightness where bloom starts. 1.0 is where the game's own bright-pass sits.\nNear 0 the whole scene glows — that is the failure mode, not a setting.");
      if (DrawResetButton(gs.BloomThreshold, default_luma_global_game_settings.BloomThreshold, "BloomThreshold"))
         device_data.cb_luma_global_settings_dirty = true;

#endif // DEVELOPMENT
      ImGui::EndDisabled();
#endif // ENABLE_BLOOM

      ImGui::SeparatorText("Effects");
      // Read in Video_0x1A82565B.ps_5_0.hlsl. Inert in SDR by construction (peak == paper white there makes
      // PumboAutoHDR an identity), so no display-mode gate is needed on either side.
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
      {
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
         device_data.cb_luma_global_settings_dirty = true;
      }
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
         // No "(HDR output)" qualifier: this dither is gated on TONEMAP_TYPE, not on the display mode, so it runs
         // in SDR too whenever that mode is on (as in the sibling Witcher 2 port).
         ImGui::SetTooltip("Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui); // Session-only: a stuck "on" would look like a broken HUD.
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mass Effect\" (2007) is developed by DristoforColumb and is open source and free.\n"
         "It adds native HDR, HDR bloom, SMAA anti-aliasing, and 16x anisotropic filtering.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
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
                  "\nOklab (hue/chroma restoration)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS)"
                  "\ndgVoodoo2 (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name;

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Mass Effect (2007) Luma HDR mod", "", mod_version);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      // scRGB fp16 swapchain (the game's backbuffer is 8-bit).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // The in-game brightness slider ([WinDrv.WindowsClient] DisplayGamma) is a D3D9 SetGammaRamp that dgVoodoo forwards
      // to the OS device ramp, which would distort the scRGB output. Exposes core's "Reset Gamma Ramp" button; the
      // vanilla-look gamma (DisplayGamma 1.6) is already folded into the grade via the readback, so nothing is lost.
      allow_disabling_gamma_ramp = true;

      // Exclusive fullscreen -> borderless. "force_borderless" is what matters on top of the core default: it
      // covers LEAVING fullscreen too, so the window cannot come back with a title bar after alt-tab (TW2).
      prevent_fullscreen_state = true;
      force_borderless = true;

      // Two families clip the HDR signal, both upgraded INDIRECTLY (a mirror substituted at bind): changing a
      // dgVoodoo resource's creation format breaks the translator's bookkeeping - black screen even in menus (MEA
      // class). MEASURED (devkit + the DEVELOPMENT DumpBoundRenderTarget lines): the canvas is r8g8b8a8_typeless and
      // both mirrors come back R16G16B16A16_FLOAT at runtime, so both entries fire. INFERRED: the gather chain's
      // CREATION format, taken from MoHA's r16g16b16a16_typeless viewed as unorm (writes clamped at 1.0) - DIAG prints
      // the post-upgrade format, which is the same fp16 for either candidate. Keyed by FORMAT, not by shader hash:
      // blits (MoHA precedent) interleave with the blurs and the last writer before the grade is a blit. The size
      // filters are load-bearing too: a stock exe caps this process at 2 GB (no LARGE_ADDRESS_AWARE bit unless a
      // patcher set it), and TW2 hit bad_alloc at 4K on a broad list.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true; // creation-time mirrors, substituted at bind (BL2/TW2 scheme)
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,     // dgVoodoo's D3D9 backbuffer surface (the LDR canvas)
         reshade::api::format::r16g16b16a16_typeless, // DoF/bloom gather, blur and blit targets, viewed as unorm
      };
      // "No1Px" is mandatory under dgVoodoo (BL2): the wrapper binds 1x1 placeholders in every unused sampler slot
      // and a 1x1 trivially passes the aspect filter, so core would mirror those too (and assert in DEVELOPMENT).
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // AF16x, an addition rather than an upgrade: the 2007 video menu has no anisotropic option at all. The
      // load-bearing line is "force_upgrade_linear_samplers" - core otherwise rewrites only ANISOTROPIC samplers and
      // this wrapper binds none (TW2 census), which made mode 4 alone a no-op there. Mip LOD bias stays 0: no TAA, so
      // it would buy shimmer.
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

      game = new MassEffect();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
