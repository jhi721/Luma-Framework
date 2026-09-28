// Katana engine PostEffect3 "cbDof" (b2), shared by the DOF passes

cbuffer cbDof : register(b2)
{
   float4 g_vDofInfo0 : packoffset(c0); // Focus distance (m) in .x, CoC scale in .y
   float4 g_vDofInfo1 : packoffset(c1); // Max CoC in .x
   float4 g_vDofInfo2 : packoffset(c2);
   float4 g_vD2Z_Z2D : packoffset(c3);   // Device depth to 1 / view z: 1 / (depth * .x + .y)
   float4 g_vTexelSize : packoffset(c4); // Half resolution DOF texel in .zw
   float4 g_vAnamorphicInfo : packoffset(c5);
   float4 g_vSampleInfo : packoffset(c6);
}
