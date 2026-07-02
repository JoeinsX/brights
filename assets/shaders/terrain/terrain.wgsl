#include "bindings.wgsl"
#include "constants.wgsl"
#include "tile.wgsl"
#include "tilemap.wgsl"
#include "heightfield.wgsl"
#include "normal.wgsl"
#include "material.wgsl"
#include "raymarch.wgsl"
#include "lib/sphere.wgsl"
#include "lib/lighting.wgsl"

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
    var corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0)
    );
    let centerNdc = vec2f(-2.0 * u_config.centerOffset.x - 1.0, 2.0 * u_config.centerOffset.y + 1.0);
    let halfExtent = (2.0 * u_config.scale * u_config.planetRadius + 4.0) / u_config.resolution;
    let lo = clamp(centerNdc - halfExtent, vec2f(-1.0), vec2f(1.0));
    let hi = clamp(centerNdc + halfExtent, vec2f(-1.0), vec2f(1.0));
    return vec4f(mix(lo, hi, corners[in_vertex_index]), u_config.planetDepth, 1.0);
}

@fragment
fn fs_main(@builtin(position) fragPos: vec4f) -> @location(0) vec4f {
    let screenPos = fragPos.xy + u_config.centerOffset * u_config.resolution;
    let viewCenter = u_config.offset;
    let sphereRadius = u_config.planetRadius;
    let bias = min(0.006 / u_config.scale, 0.005);

#ifndef ENABLE_RAYMARCHING
    let simpleModeActive = true;
    let simpleModeSmoothCoefficient = 0.0;
#else
    let simpleModeActive = u_config.scale < u_config.simpleModeThreshold;
    let simpleModeSmoothCoefficient = smoothstep(0.0, 1.0, u_config.scale - u_config.simpleModeThreshold);
#endif
    let perspectiveStrength = u_config.perspectiveStrength * simpleModeSmoothCoefficient;
    let perspectiveScale = u_config.perspectiveScale;

    var viewVec = (fragPos.xy - 0.5 * u_config.resolution) * perspectiveStrength / u_config.resolutionScale;
    viewVec *= min(1.0, 1.0 / max(length(viewVec), 0.00001));

    // project the screen pixel onto the planet sphere and find where it lands on the tile plane
    let p = ((screenPos / u_config.scale) + u_config.offset - viewCenter) / sphereRadius;
    let distSq = dot(p, p);
    if (distSq > 1.0) { discard; }

    let sphereNormal = sphereNormalFromDisk(p);
    let z = sphereNormal.z;
    let tbn = buildTbn(sphereNormal, vec3f(0.0, 1.0, 0.0));
    let worldPos = viewCenter + stereographicDiskUv(sphereNormal) * f32(mapSizeTiles) * u_config.sphereMapScale;

    let rotatedViewVec = normalize(tbn * vec3f(0.0, 0.0, -1.0)) * vec3f(viewVec, 1.0);
    let rayDir1 = normalize(vec3f(viewVec, -1.0));
    let rayDir2 = normalize(mix(rotatedViewVec, vec3f(0.0, 0.0, -1.0), distSq * distSq * 0.2));
    let rayDir = normalize(mix(rayDir1, rayDir2, distSq / 2.0));

    let trace = traceTerrain(vec3f(worldPos, maxTerrainHeight), rayDir, bias, simpleModeActive);
    if (!trace.hit) {
        return vec4f(0.1, 0.1, 0.1, 1.0);
    }

    let rayPos = trace.rayPos;
    var nh = trace.nh;
    if (!trace.nhValid) {
        nh = fetchTileNeighborhood(rayPos.xy);
    }

    // atlas LOD from the sphere->tile Jacobian footprint, plus the matching mip inset
    let zSafe = max(z, 0.001);
    let jDiag = (1.0 + z) + p * p / zSafe;
    let jCross = p.x * p.y / zSafe;
    let jToTiles = 0.5 / ((1.0 + z) * (1.0 + z)) * f32(mapSizeTiles) * u_config.sphereMapScale / (u_config.scale * sphereRadius);
    let footprintTiles = max(length(vec2f(jDiag.x, jCross)), length(vec2f(jCross, jDiag.y))) * jToTiles;
    let spriteTexels = f32(textureDimensions(t_atlas).x) / atlasGridSize.x;
    let lod = log2(max(footprintTiles * spriteTexels, 0.0001));
    let inset = min(0.5, 0.5 * exp2(ceil(max(lod, 0.0))) / spriteTexels);

    let localNormal = getAnalyticalNormalNeighborhood(rayPos.xy, nh);
    let rotatedNormal = normalize(tbn * localNormal);

#ifndef ENABLE_TRIPLANAR
    let triplanarActive = false;
#else
    let triplanarActive = nh.tiles[0].triplanar;
#endif

#ifndef ENABLE_BLENDING
    let blendingActive = false;
#else
    let blendingActive = nh.tiles[0].blending;
#endif

    var albedo: vec4f;
    if (triplanarActive) {
        if (blendingActive) {
            albedo = getBlendedTriplanarColorNeighborhood(rayPos, localNormal, perspectiveScale, lod, inset, nh);
        } else {
            albedo = getTriplanarColorNeighborhood(rayPos, localNormal, perspectiveScale, lod, inset, nh);
        }
    } else {
        if (blendingActive) {
            albedo = getBlendedColorNeighborhood(rayPos, lod, inset, nh);
        } else {
            albedo = getTerrainColor(rayPos.xy, lod, inset);
        }
    }

    let lit = applyDirectionalLight(albedo.rgb, rotatedNormal, normalize(lightDir), ambientLight);
    var color = vec4f(lit, albedo.a);

#ifdef DEBUG_NORMAL
    color = vec4f(rotatedNormal * 0.5 + 0.5, 1.0);
#endif
#ifdef DEBUG_HEIGHT
    color = vec4f(vec3f(rayPos.z * 0.5), 1.0);
#endif

    return color;
}
