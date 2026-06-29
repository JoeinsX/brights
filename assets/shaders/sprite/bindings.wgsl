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

struct SpriteInstance {
    px: f32,
    py: f32,
    pz: f32,
    rotation: f32,
    dimX: f32,
    dimY: f32,
    spriteId: u32,
};

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_instances: array<SpriteInstance>;
@group(0) @binding(2) var t_entity: texture_2d<f32>;
@group(0) @binding(3) var s_entity: sampler;
