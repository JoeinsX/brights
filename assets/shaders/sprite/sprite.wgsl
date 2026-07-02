#include "bindings.wgsl"
#include "common/worldConstants.wgsl"
#include "lib/sphere.wgsl"

const spriteSheetTileSize = 16.0;

struct VsOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
};

struct Projected {
    px: vec2f,
    p: vec2f,
    valid: bool,
};

fn projectTile(tileLocal: vec2f) -> Projected {
    let span = f32(mapSizeTiles) * u_config.sphereMapScale;
    let uv = (tileLocal - vec2f(u_config.macroOffset) - u_config.offset) / span;
    let p = sphereXyFromStereographicUv(uv);

    var result: Projected;
    result.px = p * (u_config.scale * u_config.planetRadius) - u_config.centerOffset * u_config.resolution;
    result.p = p;
    result.valid = dot(uv, uv) <= 0.25;
    return result;
}

fn viewRayDir(anchorPx: vec2f, p: vec2f, simpleSmooth: f32) -> vec3f {
    let distSq = dot(p, p);
    let zN = sqrt(max(1.0 - distSq, 0.0));

    var viewVec = (anchorPx - 0.5 * u_config.resolution) * (u_config.perspectiveStrength * simpleSmooth) / u_config.resolutionScale;
    viewVec *= min(1.0, 1.0 / max(length(viewVec), 0.00001));

    let rotatedViewVec = vec3f(-p * viewVec, -zN);
    let rayDir1 = normalize(vec3f(viewVec, -1.0));
    let rayDir2 = normalize(mix(rotatedViewVec, vec3f(0.0, 0.0, -1.0), distSq * distSq * 0.2));
    return normalize(mix(rayDir1, rayDir2, distSq * 0.5));
}

fn seatScreen(tile: vec2f, height: f32, simpleSmooth: f32) -> vec2f {
    var seat = tile;
    for (var i = 0; i < 6; i++) {
        let probe = projectTile(seat);
        let rayDir = viewRayDir(probe.px, probe.p, simpleSmooth);
        seat = tile + (maxTerrainHeight - height) * rayDir.xy / rayDir.z;
    }
    return projectTile(seat).px;
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VsOut {
    var corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0)
    );
    let corner = corners[vid];

    let inst = s_instances[iid];
    let center = vec2f(inst.px, inst.py) - vec2f(u_config.chunkOffset) * f32(chunkSize);

#ifndef ENABLE_RAYMARCHING
    let simpleSmooth = 0.0;
#else
    let simpleSmooth = smoothstep(0.0, 1.0, u_config.scale - u_config.simpleModeThreshold);
#endif

    let centerProj = projectTile(center);

    let base = seatScreen(center, inst.pz, simpleSmooth);
    let tilePx = length(seatScreen(center + vec2f(0.5, 0.0), inst.pz, simpleSmooth) - seatScreen(center - vec2f(0.5, 0.0), inst.pz, simpleSmooth));

    let cornerLocal = vec2f((corner.x - 0.5) * inst.dimX, corner.y * inst.dimY) * tilePx;
    let pivotLocal = vec2f(0.0, inst.pivotY * inst.dimY * tilePx);
    let d = cornerLocal - pivotLocal;
    let cr = cos(inst.rotation);
    let sr = sin(inst.rotation);
    let rolled = vec2f(d.x * cr - d.y * sr, d.x * sr + d.y * cr) + pivotLocal;
    let px = base + vec2f(rolled.x, -rolled.y);

    let ndc = vec2f(2.0 * px.x / u_config.resolution.x - 1.0, 1.0 - 2.0 * px.y / u_config.resolution.y);

    var out: VsOut;
    if (!centerProj.valid) {
        out.clip = vec4f(0.0, 0.0, 2.0, 1.0);
    } else {
        out.clip = vec4f(ndc, u_config.planetDepth, 1.0);
    }

    let cell = vec2f(f32(inst.spriteId >> 4u), f32(inst.spriteId & 15u));
    let cellUvSize = spriteSheetTileSize / vec2f(textureDimensions(t_entity));
    out.uv = (cell + vec2f(corner.x, 1.0 - corner.y)) * cellUvSize;
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
    let color = textureSampleLevel(t_entity, s_entity, in.uv, 0.0);
    if (color.a < 0.01) {
        discard;
    }
    return color;
}
