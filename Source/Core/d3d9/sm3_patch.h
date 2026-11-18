// Token-level patcher of D3D9 SM3 bytecode (vs_3_0 / ps_3_0 DWORD streams as
// passed to CreateVertexShader / CreatePixelShader) that adds motion vectors:
// the vertex shader runs twice (current and previous camera / world / bones),
// the pixel shader writes the UV delta to oC1. Throwaway spike code (see
// NOTES.md); header only, C++20. CTAB and comment tokens are kept.
//
// Register layout (the add-on depends on it):
//   VS c240..c243 previous camera (c8 g_ViewProjection / g_ViewProjMatrix, or
//   c24 g_WorldViewProjMatrix / Rmodelviewproj /
//                 g_WorldViewProj: whichever the VS reads), c244.. previous
//                 world c16, c248.. previous effect world c20 (row counts from
//                 the CTAB), c252 = (1 / VTF width, VTF row v, current-bones
//                 weight, VTF weight): (.., 0, 1) matched, (.., 1, 0) previous
//                 bones = current (no row needed), c253 =
//                 def'd inside the patched VS (0.5, 1.5, 2.5, 0), c255.xy =
//                 jitter in NDC. Vertex sampler s3 = A32B32G32R32F point, texel
//                 i = previous c(40+i).
//   VS outputs    o9 = texcoord14 (unjittered current clip), o10 = texcoord15
//   (previous clip), test variants o11 = texcoord13. PS            reads
//   texcoord14 / texcoord15 as v8 / v9, writes oC1.xy = (prev.xy / prev.w -
//   cur.xy / cur.w) * (0.5, -0.5),
//                 oC1.zw = 0; def c223 = (0.5, -0.5, 0, 0) inside the patched
//                 PS.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Sm3MvPatch
{

   constexpr int kPrevCameraRegister = 240;
   constexpr int kPrevWorldRegister = 244;
   constexpr int kPrevEffectWorldRegister = 248;
   constexpr int kVtfRegister = 252;
   constexpr int kBoneOffsetRegister = 253;
   constexpr int kJitterRegister = 255;
   constexpr int kFirstReservedRegister = 224;
   constexpr int kVtfSampler = 3;
   constexpr int kBoneFirstRegister = 40;
   constexpr int kBoneTexelCount = 144;
   constexpr int kUnjitteredClipOutput = 9; // dcl_texcoord14
   constexpr int kPreviousClipOutput = 10;  // dcl_texcoord15
   constexpr int kTestPositionOutput =
      11; // dcl_texcoord13 (test / capture variants)
   constexpr int kUnjitteredClipUsageIndex = 14;
   constexpr int kPreviousClipUsageIndex = 15;
   constexpr int kTestPositionUsageIndex = 13;
   constexpr int kPsClipRegisterCurrent = 8;
   constexpr int kPsClipRegisterPrevious = 9;
   constexpr int kPsHalfRegister = 223;

   // Mutation knob for the offline check: skips the camera remap, so the previous
   // position must come out wrong.
   inline bool skip_camera_remap = false;

   // Production: run 1 + run 2, o0 = jittered position, o9 / o10 = unjittered
   // current / previous clip. Test: same, but o0 = (extra input texcoord13.xy, 0,
   // 1) and the jittered run-1 position goes to o11 (texcoord13). Capture: the
   // original shader only, its position moved to o11 and o0 = the grid input (no
   // run 2, no jitter).
   enum class VsMode
   {
      Production,
      Test,
      Capture
   };

   struct VertexInfo
   {
      int camera_register = -1; // 8 or 24
      int camera_rows = 0;
      int world_register = -1; // 16
      int world_rows = 0;
      int effect_world_register = -1; // 20
      int effect_world_rows = 0;
      bool skinned = false;
      int tie_break_register =
         -1; // first of c40 (skinned), c16, c20 that the VS reads
      int test_input_register =
         -1;         // the input register of the grid position (Test / Capture)
      int slots = 0; // instruction slots of the patched shader
      int original_slots = 0;
      bool jitter_only = false; // run 2 impossible (non-bone relative addressing):
                                // jittered, texcoord15 = texcoord14
   };

   namespace Detail
   {

      namespace Op
      {
         enum : uint32_t
         {
            NOP = 0,
            MOV = 1,
            ADD = 2,
            SUB = 3,
            MAD = 4,
            MUL = 5,
            RCP = 6,
            RSQ = 7,
            DP3 = 8,
            DP4 = 9,
            MIN = 10,
            MAX = 11,
            SLT = 12,
            SGE = 13,
            EXP = 14,
            LOG = 15,
            LIT = 16,
            DST = 17,
            LRP = 18,
            FRC = 19,
            M4X4 = 20,
            M4X3 = 21,
            M3X4 = 22,
            M3X3 = 23,
            M3X2 = 24,
            CALL = 25,
            CALLNZ = 26,
            LOOP = 27,
            RET = 28,
            ENDLOOP = 29,
            LABEL = 30,
            DCL = 31,
            POW = 32,
            CRS = 33,
            SGN = 34,
            ABS = 35,
            NRM = 36,
            SINCOS = 37,
            REP = 38,
            ENDREP = 39,
            IF = 40,
            IFC = 41,
            ELSE = 42,
            ENDIF = 43,
            BREAK = 44,
            BREAKC = 45,
            MOVA = 46,
            DEFB = 47,
            DEFI = 48,
            TEXKILL = 65,
            TEXLD = 66,
            DEF = 81,
            CMP = 88,
            DP2ADD = 90,
            DSX = 91,
            DSY = 92,
            TEXLDD = 93,
            SETP = 94,
            TEXLDL = 95,
            BREAKP = 96,
            COMMENT = 0xFFFE,
            END = 0xFFFF
         };
      } // namespace Op

      enum Reg : uint32_t
      {
         TEMP = 0,
         INPUT = 1,
         CONSTF = 2,
         ADDR = 3,
         OUTPUT = 6,
         COLOROUT = 8,
         DEPTHOUT = 9,
         SAMPLER = 10,
         LOOPREG = 15
      };

      constexpr uint32_t kUsagePosition = 0, kUsageBlendIndices = 2,
                         kUsageTexcoord = 5;

      inline uint32_t RegType(uint32_t t)
      {
         return ((t >> 28) & 7) | (((t >> 11) & 3) << 3);
      }
      inline uint32_t RegNum(uint32_t t)
      {
         return t & 0x7FF;
      }
      inline bool IsRelative(uint32_t t)
      {
         return (t >> 13) & 1;
      }
      inline uint32_t SetReg(uint32_t t, uint32_t type, uint32_t num)
      {
         return (t & ~(0x70000000u | 0x1800u | 0x7FFu)) | ((type & 7) << 28) |
                (((type >> 3) & 3) << 11) | num;
      }
      inline uint32_t Dst(uint32_t type, uint32_t num, uint32_t mask = 0xF)
      {
         return SetReg(0x80000000u | (mask << 16), type, num);
      }
      inline uint32_t Src(uint32_t type, uint32_t num, uint32_t swizzle = 0xE4,
         uint32_t modifier = 0)
      {
         return SetReg(0x80000000u | (swizzle << 16) | (modifier << 24), type, num);
      }
      constexpr uint32_t Rep(uint32_t c)
      {
         return c * 0x55;
      }
      // Swizzle that only reads the components a write mask covers (the runtime
      // rejects reads of unwritten components).
      inline uint32_t MaskSwizzle(uint32_t mask)
      {
         uint32_t first = 0;
         while (first < 3 && !((mask >> first) & 1))
         {
            ++first;
         }
         uint32_t swizzle = 0;
         for (uint32_t k = 0; k < 4; ++k)
         {
            swizzle |= (((mask >> k) & 1) ? k : first) << (2 * k);
         }
         return swizzle;
      }
      inline uint32_t F2U(float f)
      {
         uint32_t u;
         std::memcpy(&u, &f, 4);
         return u;
      }

      inline const char* OpName(uint32_t op)
      {
         switch (op)
         {
         case Op::CALL:
            return "call";
         case Op::CALLNZ:
            return "callnz";
         case Op::LOOP:
            return "loop";
         case Op::RET:
            return "ret";
         case Op::ENDLOOP:
            return "endloop";
         case Op::LABEL:
            return "label";
         case Op::REP:
            return "rep";
         case Op::ENDREP:
            return "endrep";
         case Op::BREAK:
            return "break";
         case Op::BREAKC:
            return "breakc";
         case Op::BREAKP:
            return "breakp";
         default:
            return "";
         }
      }

      inline bool OpLayout(uint32_t op, int* dst, int* src)
      {
         switch (op)
         {
         case Op::NOP:
         case Op::RET:
         case Op::ENDLOOP:
         case Op::ENDREP:
         case Op::ELSE:
         case Op::ENDIF:
         case Op::BREAK:
            *dst = 0;
            *src = 0;
            return true;
         case Op::MOV:
         case Op::MOVA:
         case Op::RCP:
         case Op::RSQ:
         case Op::EXP:
         case Op::LOG:
         case Op::LIT:
         case Op::FRC:
         case Op::ABS:
         case Op::NRM:
         case Op::SINCOS:
         case Op::DSX:
         case Op::DSY:
            *dst = 1;
            *src = 1;
            return true;
         case Op::ADD:
         case Op::SUB:
         case Op::MUL:
         case Op::DP3:
         case Op::DP4:
         case Op::MIN:
         case Op::MAX:
         case Op::SLT:
         case Op::SGE:
         case Op::DST:
         case Op::M4X4:
         case Op::M4X3:
         case Op::M3X4:
         case Op::M3X3:
         case Op::M3X2:
         case Op::POW:
         case Op::CRS:
         case Op::TEXLD:
         case Op::TEXLDL:
         case Op::SETP:
            *dst = 1;
            *src = 2;
            return true;
         case Op::MAD:
         case Op::LRP:
         case Op::SGN:
         case Op::CMP:
         case Op::DP2ADD:
            *dst = 1;
            *src = 3;
            return true;
         case Op::TEXLDD:
            *dst = 1;
            *src = 4;
            return true;
         case Op::TEXKILL:
            *dst = 1;
            *src = 0;
            return true;
         case Op::IF:
         case Op::REP:
         case Op::CALL:
         case Op::LABEL:
            *dst = 0;
            *src = 1;
            return true;
         case Op::IFC:
         case Op::BREAKC:
         case Op::LOOP:
         case Op::CALLNZ:
            *dst = 0;
            *src = 2;
            return true;
         case Op::BREAKP:
            *dst = 0;
            *src = 1;
            return true;
         default:
            return false;
         }
      }

      struct Operand
      {
         size_t at; // token index of the parameter token; a relative address token
                    // follows at at + 1 when IsRelative
         bool dest;
      };

      struct Inst
      {
         size_t pos = 0;
         uint32_t op = 0;
         size_t total = 0; // tokens including the instruction token
         bool comment = false;
         std::vector<Operand> ops;
      };

      // Reads the instruction (or comment) at *i and advances. Returns false on END
      // or a malformed stream (err set for the latter).
      inline bool NextInst(const uint32_t* t, size_t n, size_t* i, Inst* in,
         std::string* err, bool* end)
      {
         *end = false;
         in->ops.clear();
         const size_t pos = *i;
         if (pos >= n)
         {
            *err = "no end token";
            return false;
         }
         const uint32_t tok = t[pos];
         const uint32_t op = tok & 0xFFFF;
         in->pos = pos;
         in->op = op;
         in->comment = false;
         if (op == Op::COMMENT)
         {
            const size_t len = (tok >> 16) & 0x7FFF;
            if (pos + 1 + len > n)
            {
               *err = "comment overruns the stream";
               return false;
            }
            in->comment = true;
            in->total = 1 + len;
            *i = pos + in->total;
            return true;
         }
         if (op == Op::END)
         {
            *end = true;
            in->total = 1;
            return true;
         }
         const size_t len = (tok >> 24) & 0xF;
         in->total = 1 + len;
         if (pos + in->total > n)
         {
            *err = "instruction overruns the stream";
            return false;
         }
         if ((tok >> 28) & 1)
         {
            *err = "predicated instruction";
            return false;
         }
         size_t idx = pos + 1;
         const size_t stop = pos + 1 + len;
         auto param = [&](bool dest)
         {
            if (idx >= stop)
               return false;
            in->ops.push_back({idx, dest});
            idx += 1 + (IsRelative(t[idx]) ? 1 : 0);
            return true;
         };
         if (op == Op::DCL)
         {
            if (len != 2)
            {
               *err = "bad dcl length";
               return false;
            }
            in->ops.push_back({pos + 2, true});
            idx = stop;
         }
         else if (op == Op::DEF || op == Op::DEFI)
         {
            if (len != 5)
            {
               *err = "bad def length";
               return false;
            }
            in->ops.push_back({pos + 1, true});
            idx = stop;
         }
         else if (op == Op::DEFB)
         {
            if (len != 2)
            {
               *err = "bad defb length";
               return false;
            }
            in->ops.push_back({pos + 1, true});
            idx = stop;
         }
         else
         {
            int nd = 0, ns = 0;
            if (!OpLayout(op, &nd, &ns))
            {
               *err = "unknown opcode " + std::to_string(op);
               return false;
            }
            for (int k = 0; k < nd; ++k)
            {
               if (!param(true))
               {
                  *err = "missing dest";
                  return false;
               }
            }
            for (int k = 0; k < ns; ++k)
            {
               if (!param(false))
               {
                  *err = "missing source";
                  return false;
               }
            }
            if (idx != stop)
            {
               *err = "operand layout mismatch (opcode " + std::to_string(op) + ")";
               return false;
            }
         }
         *i = stop;
         return true;
      }

      struct Dcl
      {
         uint32_t reg_type, reg, usage, index, tex_type;
      };
      struct RelRead
      {
         uint32_t reg, rel_type, rel_reg;
      };

      struct ShaderScan
      {
         uint32_t version = 0;
         size_t body_begin = 0, end_pos = 0;
         int max_temp = -1;
         std::vector<uint8_t> const_used = std::vector<uint8_t>(2048);
         std::vector<Dcl> dcls;
         std::vector<RelRead> rel_reads;
         std::vector<uint32_t> def_regs;
         bool writes_oc1 = false, writes_depth = false, relative_dest = false,
              mova_relative = false;
         uint32_t input_mask = 0;
         std::string flow, first_flow; // first unsupported flow op
         int slots = 0, ifs = 0, flow_ops = 0, texkills = 0;
      };

      inline bool ScanShader(const uint32_t* t, size_t n, ShaderScan* s,
         std::string* err)
      {
         if (n < 2)
         {
            *err = "too short";
            return false;
         }
         s->version = t[0];
         size_t i = 1;
         bool body_started = false;
         Inst in;
         while (true)
         {
            bool end = false;
            if (!NextInst(t, n, &i, &in, err, &end))
               return false;
            if (end)
            {
               s->end_pos = in.pos;
               if (!body_started)
               {
                  s->body_begin = in.pos;
               }
               return true;
            }
            if (in.comment)
               continue;
            const uint32_t op = in.op;
            const bool header =
               op == Op::DCL || op == Op::DEF || op == Op::DEFI || op == Op::DEFB;
            if (!header && !body_started)
            {
               body_started = true;
               s->body_begin = in.pos;
            }
            if (op == Op::DCL)
            {
               const uint32_t usage_tok = t[in.pos + 1], dst = t[in.pos + 2];
               s->dcls.push_back({RegType(dst), RegNum(dst), usage_tok & 0x1F,
                  (usage_tok >> 16) & 0xF, (usage_tok >> 27) & 0xF});
               continue;
            }
            if (op == Op::DEF)
            {
               s->def_regs.push_back(RegNum(t[in.pos + 1]));
               continue;
            }
            if (header)
               continue;
            ++s->slots;
            if (*OpName(op))
            {
               ++s->flow_ops;
               if (s->first_flow.empty())
               {
                  s->first_flow = OpName(op);
               }
               // subroutines are refused; rep / loop / break stay plain re-emission
               if (s->flow.empty() && (op == Op::CALL || op == Op::CALLNZ ||
                                         op == Op::LABEL || op == Op::RET))
               {
                  s->flow = OpName(op);
               }
            }
            if (op == Op::IF || op == Op::IFC)
            {
               ++s->ifs;
            }
            if (op == Op::TEXKILL)
            {
               ++s->texkills;
            }
            for (const Operand& o : in.ops)
            {
               const uint32_t tok = t[o.at], type = RegType(tok), num = RegNum(tok);
               const bool rel = IsRelative(tok);
               if (o.dest)
               {
                  if (rel)
                  {
                     s->relative_dest = true;
                  }
                  if (type == TEMP)
                  {
                     s->max_temp = std::max<int>(s->max_temp, int(num));
                  }
                  if (type == COLOROUT && num == 1)
                  {
                     s->writes_oc1 = true;
                  }
                  if (type == DEPTHOUT)
                  {
                     s->writes_depth = true;
                  }
               }
               else
               {
                  if (type == TEMP)
                  {
                     s->max_temp = std::max<int>(s->max_temp, int(num));
                  }
                  if (type == INPUT && num < 32)
                  {
                     s->input_mask |= 1u << num;
                  }
                  if (type == CONSTF)
                  {
                     if (num < 2048)
                     {
                        s->const_used[num] = 1;
                     }
                     if (rel)
                     {
                        const uint32_t r = t[o.at + 1];
                        s->rel_reads.push_back({num, RegType(r), RegNum(r)});
                     }
                  }
                  if (op == Op::MOVA && rel)
                  {
                     s->mova_relative = true;
                  }
               }
            }
         }
      }

      struct CtabEntry
      {
         std::string name;
         uint32_t set, reg, count;
      };

      inline std::vector<CtabEntry> ReadCtab(const uint32_t* t, size_t n)
      {
         std::vector<CtabEntry> list;
         size_t i = 1;
         Inst in;
         std::string err;
         bool end = false;
         while (NextInst(t, n, &i, &in, &err, &end) && !end)
         {
            if (!in.comment || in.total < 2 || t[in.pos + 1] != 0x42415443)
               continue; // 'CTAB'
            const uint8_t* base = reinterpret_cast<const uint8_t*>(&t[in.pos + 2]);
            const size_t bytes = (in.total - 2) * 4;
            auto u32 = [&](size_t off)
            {
               uint32_t v = 0;
               if (off + 4 <= bytes)
               {
                  std::memcpy(&v, base + off, 4);
               }
               return v;
            };
            auto u16 = [&](size_t off)
            {
               uint16_t v = 0;
               if (off + 2 <= bytes)
               {
                  std::memcpy(&v, base + off, 2);
               }
               return uint32_t(v);
            };
            const uint32_t count = u32(12), info = u32(16);
            for (uint32_t k = 0; k < count && k < 4096; ++k)
            {
               const size_t e = info + size_t(k) * 20;
               if (e + 20 > bytes)
                  break;
               CtabEntry c{};
               c.set = u16(e + 4);
               c.reg = u16(e + 6);
               c.count = u16(e + 8);
               for (size_t p = u32(e); p < bytes && base[p]; ++p)
               {
                  c.name.push_back(char(base[p]));
               }
               list.push_back(std::move(c));
            }
            break;
         }
         return list;
      }

      inline bool IsCameraName(uint32_t reg, const std::string& name)
      {
         if (reg == 8)
            return name == "g_ViewProjection" || name == "g_ViewProjMatrix";
         if (reg == 24)
            return name == "g_WorldViewProjMatrix" || name == "Rmodelviewproj" ||
                   name == "g_WorldViewProj";
         return false;
      }

   } // namespace Detail

   inline bool PatchVertexShader(const uint32_t* t, size_t n, VsMode mode,
      std::vector<uint32_t>* out, VertexInfo* info,
      std::string* error, size_t max_slots = 0)
   {
      using namespace Detail;
      std::string err;
      auto fail = [&](const std::string& m)
      {
         if (error)
         {
            *error = m;
         }
         return false;
      };
      ShaderScan s;
      if (!ScanShader(t, n, &s, &err))
         return fail("unparsed: " + err);
      if (s.version != 0xFFFE0300)
         return fail("not vs_3_0");
      if (!s.flow.empty())
         return fail("unsupported flow control: " + s.flow);
      if (s.relative_dest)
         return fail("relative destination");
      if (s.mova_relative)
         return fail("relative mova source");

      int pos_out = -1, max_in = -1;
      for (const Dcl& d : s.dcls)
      {
         if (d.reg_type == OUTPUT)
         {
            if (d.reg >= uint32_t(kUnjitteredClipOutput))
               return fail("output register o9+ used");
            if (d.usage == kUsageTexcoord && d.index >= 13)
               return fail("texcoord13-15 output used");
            if (d.usage == kUsagePosition && d.index == 0)
            {
               pos_out = int(d.reg);
            }
         }
         else if (d.reg_type == INPUT)
         {
            max_in = std::max<int>(max_in, int(d.reg));
            if (d.usage == kUsageTexcoord && d.index == kTestPositionUsageIndex)
               return fail("texcoord13 input used");
         }
         else if (d.reg_type == SAMPLER && d.reg == uint32_t(kVtfSampler))
            return fail("sampler s3 used");
      }
      if (pos_out < 0)
         return fail("no position output");
      for (size_t r = kFirstReservedRegister; r < s.const_used.size(); ++r)
      {
         if (s.const_used[r])
            return fail("constant >= c224 used");
      }
      for (uint32_t r : s.def_regs)
      {
         if (r >= uint32_t(kFirstReservedRegister))
            return fail("constant >= c224 used");
      }
      bool jitter_only = false;
      for (const RelRead& r : s.rel_reads)
      {
         if (r.reg < uint32_t(kBoneFirstRegister) ||
             r.reg > uint32_t(kBoneFirstRegister) + 2 || r.rel_type != ADDR ||
             r.rel_reg != 0)
         {
            jitter_only = true; // instanced particle arrays: no run 2, previous clip
                                // = current clip
         }
      }
      int test_input = -1;
      if (mode != VsMode::Production)
      {
         test_input = max_in + 1;
         if (test_input > 15)
            return fail("no free input register");
      }
      const int base = s.max_temp + 1;
      if (base + 9 > 32)
         return fail("no free temps");
      const uint32_t rP = base, rA = base + 1, rT = base + 2, rS = base + 3,
                     rF0 = base + 4, rU = base + 8;
      constexpr int kFetchTemps = 4;

      VertexInfo vi;
      vi.skinned = !s.rel_reads.empty();
      vi.original_slots = s.slots;
      vi.test_input_register = test_input;
      const std::vector<CtabEntry> ctab = ReadCtab(t, n);
      auto reads = [&](uint32_t reg, uint32_t count)
      {
         for (uint32_t r = reg; r < reg + count && r < s.const_used.size(); ++r)
         {
            if (s.const_used[r])
               return true;
         }
         return false;
      };
      int cam8 = -1, cam24 = -1;
      for (const CtabEntry& c : ctab)
      {
         if (c.set != 2 || c.count == 0)
            continue;
         const uint32_t rows = std::min<uint32_t>(c.count, 4);
         if (IsCameraName(c.reg, c.name) && reads(c.reg, rows))
         {
            (c.reg == 8 ? cam8 : cam24) = int(rows);
         }
         if (c.reg == 16 && reads(16, rows) && vi.world_register < 0)
         {
            vi.world_register = 16;
            vi.world_rows = int(rows);
         }
         if (c.reg == 20 && reads(20, rows) && vi.effect_world_register < 0)
         {
            vi.effect_world_register = 20;
            vi.effect_world_rows = int(rows);
         }
      }
      if (cam8 >= 0 && cam24 >= 0)
         return fail("reads both cameras");
      if (cam8 >= 0)
      {
         vi.camera_register = 8;
         vi.camera_rows = cam8;
      }
      if (cam24 >= 0)
      {
         vi.camera_register = 24;
         vi.camera_rows = cam24;
      }
      vi.tie_break_register = vi.skinned                      ? kBoneFirstRegister
                              : vi.world_register >= 0        ? 16
                              : vi.effect_world_register >= 0 ? 20
                                                              : -1;

      vi.jitter_only = jitter_only;
      const bool run2 = mode != VsMode::Capture && !jitter_only;
      auto remap = [&](uint32_t reg) -> uint32_t
      {
         if (vi.camera_register >= 0 && !skip_camera_remap &&
             reg >= uint32_t(vi.camera_register) &&
             reg < uint32_t(vi.camera_register + vi.camera_rows))
         {
            return kPrevCameraRegister + (reg - vi.camera_register);
         }
         if (vi.world_register >= 0 && reg >= 16 &&
             reg < uint32_t(16 + vi.world_rows))
            return kPrevWorldRegister + (reg - 16);
         if (vi.effect_world_register >= 0 && reg >= 20 &&
             reg < uint32_t(20 + vi.effect_world_rows))
         {
            return kPrevEffectWorldRegister + (reg - 20);
         }
         return reg;
      };

      std::vector<uint32_t> o;
      o.reserve(n * 2 + 64);
      auto ins = [&](uint32_t op, std::initializer_list<uint32_t> a)
      {
         o.push_back(op | (uint32_t(a.size()) << 24));
         o.insert(o.end(), a.begin(), a.end());
      };
      o.push_back(t[0]);
      // The runtime wants every def before the first dcl: the new def goes right
      // before it.
      size_t first_dcl = s.body_begin;
      Inst in;
      {
         size_t i = 1;
         bool end = false;
         while (i < s.body_begin && NextInst(t, n, &i, &in, &err, &end) && !end)
         {
            if (!in.comment && in.op == Op::DCL)
            {
               first_dcl = in.pos;
               break;
            }
         }
      }
      o.insert(o.end(), t + 1, t + first_dcl);
      const uint32_t full = 0xF;
      if (run2 && vi.skinned)
      {
         ins(Op::DEF, {Dst(CONSTF, kBoneOffsetRegister), F2U(0.5f), F2U(1.5f),
                         F2U(2.5f), F2U(0.0f)});
      }
      {
         // Test / capture variants drop the original outputs but the position
         // (vs_3_0 has 12; the test needs o9..o11 beside o0..o8)
         size_t i = first_dcl;
         bool end = false;
         while (i < s.body_begin && NextInst(t, n, &i, &in, &err, &end) && !end)
         {
            const bool drop = mode != VsMode::Production && !in.comment &&
                              in.op == Op::DCL && RegType(t[in.pos + 2]) == OUTPUT &&
                              RegNum(t[in.pos + 2]) != uint32_t(pos_out);
            if (!drop)
            {
               o.insert(o.end(), t + in.pos, t + in.pos + in.total);
            }
         }
      }
      if (run2 && vi.skinned)
      {
         ins(Op::DCL, {0x90000000u, Dst(SAMPLER, kVtfSampler, full)});
      }
      auto dcl_usage = [&](uint32_t idx)
      {
         return 0x80000000u | kUsageTexcoord | (idx << 16);
      };
      if (mode != VsMode::Capture)
      {
         ins(Op::DCL, {dcl_usage(kUnjitteredClipUsageIndex),
                         Dst(OUTPUT, kUnjitteredClipOutput, full)});
         ins(Op::DCL, {dcl_usage(kPreviousClipUsageIndex),
                         Dst(OUTPUT, kPreviousClipOutput, full)});
      }
      if (mode != VsMode::Production)
      {
         ins(Op::DCL, {dcl_usage(kTestPositionUsageIndex),
                         Dst(OUTPUT, kTestPositionOutput, full)});
         ins(Op::DCL,
            {dcl_usage(kTestPositionUsageIndex), Dst(INPUT, test_input, full)});
      }

      // Run 1: the original stream, the position output redirected into rP.
      {
         size_t i = s.body_begin;
         while (i < s.end_pos)
         {
            bool end = false;
            if (!NextInst(t, n, &i, &in, &err, &end))
               return fail("unparsed: " + err);
            const size_t first = o.size();
            o.insert(o.end(), t + in.pos, t + in.pos + in.total);
            for (const Operand& op : in.ops)
            {
               const uint32_t tok = t[op.at];
               if (op.dest && RegType(tok) == OUTPUT)
               {
                  const bool is_position = RegNum(tok) == uint32_t(pos_out);
                  if (is_position || mode != VsMode::Production)
                  {
                     o[first + (op.at - in.pos)] =
                        SetReg(tok, TEMP, is_position ? rP : rS);
                  }
               }
            }
         }
      }
      if (mode == VsMode::Capture)
      {
         ins(Op::MOV, {Dst(OUTPUT, kTestPositionOutput), Src(TEMP, rP)});
      }
      else
      {
         ins(Op::MOV, {Dst(OUTPUT, kUnjitteredClipOutput), Src(TEMP, rP)});
         const uint32_t target =
            mode == VsMode::Production ? pos_out : kTestPositionOutput;
         ins(Op::MAD, {Dst(OUTPUT, target, 3), Src(CONSTF, kJitterRegister),
                         Src(TEMP, rP, Rep(3)), Src(TEMP, rP)});
         ins(Op::MOV, {Dst(OUTPUT, target, 0xC), Src(TEMP, rP)});
      }

      // Run 2: previous camera / world constants, previous bones from the vertex
      // texture.
      if (run2)
      {
         size_t i = s.body_begin;
         while (i < s.end_pos)
         {
            bool end = false;
            if (!NextInst(t, n, &i, &in, &err, &end))
               return fail("unparsed: " + err);
            if (in.comment)
            {
               o.insert(o.end(), t + in.pos, t + in.pos + in.total);
               continue;
            }
            if (in.op == Op::MOVA)
            {
               const uint32_t dtok = t[in.pos + 1];
               if (RegType(dtok) != ADDR)
                  return fail("mova to a non-address register");
               const uint32_t mask = (dtok >> 16) & 0xF;
               const uint32_t src = t[in.pos + 2];
               // rA = floor(src + 0.5): mova rounds to nearest
               ins(Op::ADD, {Dst(TEMP, rA, mask), src,
                               Src(CONSTF, kBoneOffsetRegister, Rep(0))});
               ins(Op::FRC, {Dst(TEMP, rT, mask), Src(TEMP, rA, MaskSwizzle(mask))});
               ins(Op::ADD, {Dst(TEMP, rA, mask), Src(TEMP, rA, MaskSwizzle(mask)),
                               Src(TEMP, rT, MaskSwizzle(mask), 1)});
               // a0 again for the current-bone reads below
               o.insert(o.end(), t + in.pos, t + in.pos + in.total);
               continue;
            }
            std::vector<uint32_t> body(t + in.pos, t + in.pos + in.total);
            std::vector<uint8_t> drop(in.total, 0);
            int src_index = 0;
            for (const Operand& op : in.ops)
            {
               const size_t k = op.at - in.pos;
               const uint32_t tok = t[op.at], type = RegType(tok), num = RegNum(tok);
               if (op.dest)
               {
                  if (type == OUTPUT)
                  {
                     body[k] = SetReg(tok, TEMP, num == uint32_t(pos_out) ? rP : rS);
                  }
                  continue;
               }
               const int slot = src_index++;
               if (type != CONSTF)
                  continue;
               if (!IsRelative(tok))
               {
                  body[k] = SetReg(tok, CONSTF, remap(num));
                  continue;
               }
               if (slot >= kFetchTemps)
                  return fail("too many relative sources");
               const uint32_t component = (t[op.at + 1] >> 16) & 3,
                              bone_row = num - kBoneFirstRegister, rF = rF0 + slot;
               ins(Op::ADD, {Dst(TEMP, rU, 1), Src(TEMP, rA, Rep(component)),
                               Src(CONSTF, kBoneOffsetRegister, Rep(bone_row))});
               ins(Op::MUL, {Dst(TEMP, rU, 1), Src(TEMP, rU, Rep(0)),
                               Src(CONSTF, kVtfRegister, Rep(0))});
               ins(Op::MOV, {Dst(TEMP, rU, 2), Src(CONSTF, kVtfRegister, Rep(1))});
               ins(Op::MOV,
                  {Dst(TEMP, rU, 0xC), Src(CONSTF, kBoneOffsetRegister, Rep(3))});
               ins(Op::TEXLDL,
                  {Dst(TEMP, rF, full), Src(TEMP, rU), Src(SAMPLER, kVtfSampler)});
               // previous = vtf * c252.w + current * c252.z: exact for (0, 1) / (1, 0)
               // (lrp may round); an unmatched draw reuses its current bones
               ins(Op::MOV, {Dst(TEMP, rU, full), (tok & ~0x0FFF0000u) | (0xE4u << 16),
                               t[op.at + 1]});
               ins(Op::MUL, {Dst(TEMP, rU, full), Src(TEMP, rU),
                               Src(CONSTF, kVtfRegister, Rep(2))});
               ins(Op::MAD, {Dst(TEMP, rF, full), Src(TEMP, rF),
                               Src(CONSTF, kVtfRegister, Rep(3)), Src(TEMP, rU)});
               body[k] = SetReg(tok & ~(1u << 13), TEMP, rF);
               drop[k + 1] = 1;
            }
            const size_t first = o.size();
            for (size_t k = 0; k < body.size(); ++k)
            {
               if (!drop[k])
               {
                  o.push_back(body[k]);
               }
            }
            o[first] =
               (o[first] & ~(0xFu << 24)) | (uint32_t(o.size() - first - 1) << 24);
         }
      }
      if (mode == VsMode::Production)
      {
         ins(Op::MOV, {Dst(OUTPUT, kPreviousClipOutput), Src(TEMP, rP)});
      }
      if (mode == VsMode::Test)
      {
         ins(Op::MOV, {Dst(OUTPUT, kPreviousClipOutput), Src(TEMP, rP)});
      }
      if (mode != VsMode::Production)
      {
         ins(Op::MOV, {Dst(OUTPUT, pos_out), Src(INPUT, test_input)});
      }
      o.push_back(0x0000FFFF);

      ShaderScan check;
      if (!ScanShader(o.data(), o.size(), &check, &err))
         return fail("internal: patched stream unparsable: " + err);
      vi.slots = check.slots;
      if (max_slots && size_t(vi.slots) > max_slots)
         return fail("too many instruction slots");
      if (info)
      {
         *info = vi;
      }
      if (out)
      {
         *out = std::move(o);
      }
      return true;
   }

   inline bool PatchPixelShader(const uint32_t* t, size_t n,
      std::vector<uint32_t>* out, std::string* error)
   {
      using namespace Detail;
      std::string err;
      auto fail = [&](const std::string& m)
      {
         if (error)
         {
            *error = m;
         }
         return false;
      };
      ShaderScan s;
      if (!ScanShader(t, n, &s, &err))
         return fail("unparsed: " + err);
      if (s.version != 0xFFFF0300)
         return fail("not ps_3_0");
      if (s.writes_oc1)
         return fail("writes oC1");
      if (s.writes_depth)
         return fail("writes oDepth");
      if (!s.flow.empty())
         return fail("unsupported flow control: " + s.flow);
      if (s.input_mask &
          ((1u << kPsClipRegisterCurrent) | (1u << kPsClipRegisterPrevious)))
         return fail("uses v8/v9");
      for (const Dcl& d : s.dcls)
      {
         if (d.reg_type == INPUT && (d.reg == uint32_t(kPsClipRegisterCurrent) ||
                                       d.reg == uint32_t(kPsClipRegisterPrevious)))
         {
            return fail("uses v8/v9");
         }
      }
      for (size_t r = kPsHalfRegister; r < s.const_used.size(); ++r)
      {
         if (s.const_used[r])
            return fail("constant c223+ used");
      }
      for (uint32_t r : s.def_regs)
      {
         if (r >= uint32_t(kPsHalfRegister))
            return fail("constant c223+ used");
      }
      const int base = s.max_temp + 1;
      if (base + 2 > 32)
         return fail("no free temps");
      const uint32_t ta = base, tb = base + 1;

      std::vector<uint32_t> o;
      o.reserve(n + 64);
      auto ins = [&](uint32_t op, std::initializer_list<uint32_t> a)
      {
         o.push_back(op | (uint32_t(a.size()) << 24));
         o.insert(o.end(), a.begin(), a.end());
      };
      o.insert(o.end(), t, t + s.body_begin);
      ins(Op::DEF, {Dst(CONSTF, kPsHalfRegister), F2U(0.5f), F2U(-0.5f), F2U(0.0f),
                      F2U(0.0f)});
      ins(Op::DCL, {0x80000000u | kUsageTexcoord |
                         (uint32_t(kUnjitteredClipUsageIndex) << 16),
                      Dst(INPUT, kPsClipRegisterCurrent)});
      ins(Op::DCL,
         {0x80000000u | kUsageTexcoord | (uint32_t(kPreviousClipUsageIndex) << 16),
            Dst(INPUT, kPsClipRegisterPrevious)});
      o.insert(o.end(), t + s.body_begin, t + s.end_pos);
      ins(Op::RCP, {Dst(TEMP, ta, 1), Src(INPUT, kPsClipRegisterCurrent, Rep(3))});
      ins(Op::RCP, {Dst(TEMP, ta, 2), Src(INPUT, kPsClipRegisterPrevious, Rep(3))});
      ins(Op::MUL, {Dst(TEMP, tb, 3), Src(INPUT, kPsClipRegisterCurrent),
                      Src(TEMP, ta, Rep(0))});
      ins(Op::MAD, {Dst(TEMP, tb, 3), Src(INPUT, kPsClipRegisterPrevious),
                      Src(TEMP, ta, Rep(1)), Src(TEMP, tb, 0xE4, 1)});
      ins(Op::MUL,
         {Dst(COLOROUT, 1, 3), Src(TEMP, tb), Src(CONSTF, kPsHalfRegister)});
      ins(Op::MOV, {Dst(COLOROUT, 1, 0xC), Src(CONSTF, kPsHalfRegister, Rep(2))});
      o.push_back(0x0000FFFF);

      ShaderScan check;
      if (!ScanShader(o.data(), o.size(), &check, &err))
         return fail("internal: patched stream unparsable: " + err);
      if (out)
      {
         *out = std::move(o);
      }
      return true;
   }

   // The DX11 mod's DXBC::ClampOutput0 on a ps_3_0 stream (Bayonetta's
   // upgraded fp16 targets don't clamp what the 8-bit ones did): every oC0 write
   // goes to a free temp, the tail writes oC0 from it. Blend (BLEND_INPUT): rgb
   // >= 0, alpha in [0, 1]. Unorm8 (the velocity target): saturate and round to
   // the 8-bit step. Saturate (an 8-bit UNORM target's clamp, DXBC's
   // ClampOutput0 with every channel in [0, 1]): rgba in [0, 1], no rounding.
   // The constants go to c222 (under the motion vector patch's c223, over the
   // game's and the injected c208 block).
   enum class OutputClamp
   {
      Blend,
      Unorm8,
      Saturate
   };
   constexpr int kClampRegister = 222;

   inline bool PatchOutputClamp(const uint32_t* t, size_t n, OutputClamp mode,
      std::vector<uint32_t>* out, std::string* error)
   {
      using namespace Detail;
      std::string err;
      auto fail = [&](const std::string& m)
      {
         if (error)
         {
            *error = m;
         }
         return false;
      };
      ShaderScan s;
      if (!ScanShader(t, n, &s, &err))
         return fail("unparsed: " + err);
      if (s.version != 0xFFFF0300)
         return fail("not ps_3_0");
      if (!s.flow.empty())
         return fail("unsupported flow control: " + s.flow);
      if (s.const_used[kClampRegister])
         return fail("c222 used");
      for (uint32_t r : s.def_regs)
      {
         if (r == uint32_t(kClampRegister))
            return fail("c222 used");
      }
      const uint32_t value = s.max_temp + 1, fraction = s.max_temp + 2;
      if (fraction >= 32)
         return fail("no free temps");

      std::vector<uint32_t> o;
      o.reserve(n + 32);
      auto ins = [&](uint32_t op, std::initializer_list<uint32_t> a)
      {
         o.push_back(op | (uint32_t(a.size()) << 24));
         o.insert(o.end(), a.begin(), a.end());
      };
      o.insert(o.end(), t, t + s.body_begin);
      ins(Op::DEF, {Dst(CONSTF, kClampRegister), F2U(0.0f), F2U(255.0f),
                      F2U(0.5f), F2U(1.0f / 255.0f)});
      uint32_t mask = 0;
      Inst in;
      size_t i = s.body_begin;
      while (i < s.end_pos)
      {
         bool end = false;
         if (!NextInst(t, n, &i, &in, &err, &end))
            return fail("unparsed: " + err);
         const size_t first = o.size();
         o.insert(o.end(), t + in.pos, t + in.pos + in.total);
         for (const Operand& op : in.ops)
         {
            const uint32_t tok = t[op.at];
            if (op.dest && RegType(tok) == COLOROUT && RegNum(tok) == 0)
            {
               mask |= (tok >> 16) & 0xF;
               o[first + (op.at - in.pos)] = SetReg(tok, TEMP, value);
            }
         }
      }
      if (!mask)
         return fail("no oC0 write");
      constexpr uint32_t kSaturate = 1u << 20;
      switch (mode)
      {
      case OutputClamp::Blend:
         if (mask & 7)
         {
            ins(Op::MAX, {Dst(COLOROUT, 0, mask & 7), Src(TEMP, value),
                            Src(CONSTF, kClampRegister, Rep(0))});
         }
         if (mask & 8)
         {
            ins(Op::MOV, {Dst(COLOROUT, 0, 8) | kSaturate, Src(TEMP, value)});
         }
         break;
      case OutputClamp::Saturate:
         ins(Op::MOV, {Dst(COLOROUT, 0, mask) | kSaturate, Src(TEMP, value)});
         break;
      case OutputClamp::Unorm8:
         ins(Op::MOV, {Dst(TEMP, value, mask) | kSaturate, Src(TEMP, value)});
         ins(Op::MAD, {Dst(TEMP, value, mask), Src(TEMP, value),
                         Src(CONSTF, kClampRegister, Rep(1)),
                         Src(CONSTF, kClampRegister, Rep(2))});
         ins(Op::FRC, {Dst(TEMP, fraction, mask), Src(TEMP, value)});
         ins(Op::ADD, {Dst(TEMP, value, mask), Src(TEMP, value),
                         Src(TEMP, fraction, 0xE4, 1)});
         ins(Op::MUL, {Dst(COLOROUT, 0, mask), Src(TEMP, value),
                         Src(CONSTF, kClampRegister, Rep(3))});
         break;
      }
      o.push_back(0x0000FFFF);

      ShaderScan check;
      if (!ScanShader(o.data(), o.size(), &check, &err))
         return fail("internal: patched stream unparsable: " + err);
      if (out)
      {
         *out = std::move(o);
      }
      return true;
   }

} // namespace Sm3MvPatch
