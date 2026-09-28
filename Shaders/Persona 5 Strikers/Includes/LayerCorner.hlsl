#ifndef SRC_P5S_LAYER_CORNER_HLSL
#define SRC_P5S_LAYER_CORNER_HLSL

// 3D layers drawn at the output resolution (see "DrawLayerAtOutputResolution"): texture coordinates into their targets within the
// render resolution corner are scaled to the whole target; those beyond it (already the whole target) are kept.
cbuffer LayerScale : register(b5)
{
   float2 uvScale; // Output resolution / render resolution
};

// ponytail: a sub-rect inside the corner of a full resolution texture the layer doesn't draw would be scaled too; none are known
float2 LayerCornerToWhole(float2 uv)
{
   return uv <= 1.01 / uvScale ? uv * uvScale : uv;
}

#endif // SRC_P5S_LAYER_CORNER_HLSL
