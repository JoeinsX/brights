#include "common/uniforms.wgsl"

// must match SpriteInstance in src/core/world/graphics/spriteInstance.hpp
struct SpriteInstance {
    px: f32,
    py: f32,
    pz: f32,
    rotation: f32,
    dimX: f32,
    dimY: f32,
    spriteId: u32,
    pivotY: f32,
};

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_instances: array<SpriteInstance>;
@group(0) @binding(2) var t_entity: texture_2d<f32>;
@group(0) @binding(3) var s_entity: sampler;
