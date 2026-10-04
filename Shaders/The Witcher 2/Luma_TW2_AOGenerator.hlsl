// The native HBAO generator (PS 0x3FEEC0F7 / 0x6EC596CA) over the whole AO target under the render scale (main.cpp
// "RunNativeAOWholeTarget"). The generator builds its NDC from its UV (v5 * (2, -2) + (-1, 1)) and its pixel radius from the AO
// allocation's size (cb4[8]), so it is only right when v5 spans [0, 1] over the whole target. The game's pass through VS
// (0x5D9D0449) feeds it the rendered share's UV instead ([0, area] under the render scale): a darker, noisier AO bent toward the
// top-left. This VS draws one triangle over the whole target with v5 over [0, 1], and the generator's depth (t0) and view normals (t2,
// a full surface G-buffer target it samples at the AO grid's UV) are stretched to the target ("point_stretch_ps").

// dgVoodoo's 13 VS outputs in the generator's input order (both builds); only the position and TEXCOORD0 are read, the rest carry the
// game VS's constants
void fullscreen_dgv_vs(
    uint vertex_id : SV_VertexID,
    out float4 o0 : SV_POSITION0,
    out float4 o1 : TEXCOORD8,
    out float4 o2 : COLOR0,
    out float4 o3 : COLOR1,
    out float4 o4 : TEXCOORD9,
    out float4 o5 : TEXCOORD0,
    out float4 o6 : TEXCOORD1,
    out float4 o7 : TEXCOORD2,
    out float4 o8 : TEXCOORD3,
    out float4 o9 : TEXCOORD4,
    out float4 o10 : TEXCOORD5,
    out float4 o11 : TEXCOORD6,
    out float4 o12 : TEXCOORD7)
{
   const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
   o0 = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.5, 1.0);
   o1 = o0;
   o2 = 1.0;
   o3 = float4(0.0, 0.0, 0.0, 1.0);
   o4 = float4(1.0, 0.0, 0.0, 0.0);
   o5 = float4(uv, 0.0, 1.0);
   o6 = float4(0.0, 0.0, 0.0, 1.0);
   o7 = o6;
   o8 = o6;
   o9 = o6;
   o10 = o6;
   o11 = o6;
   o12 = o6;
}
