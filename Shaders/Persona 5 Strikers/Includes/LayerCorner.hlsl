#ifndef SRC_P5S_LAYER_CORNER_HLSL
#define SRC_P5S_LAYER_CORNER_HLSL

// The 3D layers drawn at the output resolution (see "DrawLayerAtOutputResolution"): the game's texture coordinates into their targets
// that address the render resolution corner are scaled to the whole target, those beyond it (a whole target) are kept.
cbuffer LayerScale : register(b5)
{
   float2 uvScale; // Output resolution / render resolution
};

// ponytail: a coordinate inside the corner of a texture the layer doesn't draw (a sub-rect of a full resolution texture) would be scaled too; none are known
float2 LayerCornerToWhole(float2 uv)
{
   return uv <= 1.01 / uvScale ? uv * uvScale : uv;
}

#endif // SRC_P5S_LAYER_CORNER_HLSL
