// sample one tile from an atlas, insetting the inner uv to keep neighbor tiles from bleeding at the chosen LOD
fn sampleSubTile(tex: texture_2d<f32>, samp: sampler, gridSize: vec2f, spriteUv: vec2f, innerUv: vec2f, lod: f32, inset: f32) -> vec4f {
    let uv = clamp(innerUv, vec2f(inset), vec2f(1.0 - inset));
    return textureSampleLevel(tex, samp, (spriteUv + uv) / gridSize, lod);
}
