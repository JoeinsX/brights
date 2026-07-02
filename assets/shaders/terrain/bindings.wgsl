#include "common/uniforms.wgsl"

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_tilemap: array<u32>;
@group(0) @binding(2) var t_atlas: texture_2d<f32>;
@group(0) @binding(3) var s_atlas: sampler;
@group(0) @binding(4) var<storage, read> s_packed: array<u32>;
