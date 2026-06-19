struct Uniforms {
    macroOffset: vec2i,
    offset: vec2f,
    centerOffset: vec2f,
    resolution: vec2f,
    scale: f32,
    sphereMapScale: f32,
    chunkOffset: vec2i,
    resolutionScale: vec2f,
    perspectiveStrength: f32,
    perspectiveScale: f32,
    planetRadius: f32,
    planetDepth: f32,
    simpleModeThreshold: f32,
    raymarchMaxTiles: i32,
    raymarchBinarySteps: i32,
    _pad: f32,
};

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_tilemap: array<u32>;
@group(0) @binding(2) var t_atlas: texture_2d<f32>;
@group(0) @binding(3) var s_atlas: sampler;
@group(0) @binding(4) var<storage, read> s_packed: array<u32>;
