#include "bindings.wgsl"
#include "constants.wgsl"
#include "tile.wgsl"
#include "tilemap.wgsl"
#include "lib/sampling.wgsl"

struct WeightedColor {
    color: vec4f,
    weight: f32,
};

fn sampleTile(spriteUv: vec2f, innerUv: vec2f, lod: f32, inset: f32) -> vec4f {
    return sampleSubTile(t_atlas, s_atlas, atlasGridSize, spriteUv, innerUv, lod, inset);
}

// the four tiles whose footprints overlap the half-tile-shifted sample, ordered TL, TR, BL, BR
fn selectQuadrant(nh: TileNeighborhood, uvZ: vec2f) -> array<TileData, 4> {
    let isRight = uvZ.x >= 0.5;
    let isDown = uvZ.y >= 0.5;

    if (isRight) {
        if (isDown) {
            return array<TileData, 4>(nh.tiles[0], nh.tiles[2], nh.tiles[8], nh.tiles[1]);
        }
        return array<TileData, 4>(nh.tiles[4], nh.tiles[3], nh.tiles[0], nh.tiles[2]);
    }
    if (isDown) {
        return array<TileData, 4>(nh.tiles[6], nh.tiles[0], nh.tiles[7], nh.tiles[8]);
    }
    return array<TileData, 4>(nh.tiles[5], nh.tiles[4], nh.tiles[6], nh.tiles[0]);
}

fn getTerrainColor(pos: vec2f, lod: f32, inset: f32) -> vec4f {
    let idTop = fetchTileUv(floor(pos)).xy;
    let uvTop = fract(pos);
    return sampleTile(idTop, uvTop, lod, inset);
}

fn getBlendedColorNeighborhood(worldPos: vec3f, lod: f32, inset: f32, nh: TileNeighborhood) -> vec4f {
    let uvZ = fract(worldPos.xy);
    let gridPos = worldPos.xy - 0.5;
    let baseTile = floor(gridPos);
    let f = fract(gridPos);

    let q = selectQuadrant(nh, uvZ);
    let sVals = vec4f(q[0].softness, q[1].softness, q[2].softness, q[3].softness);
    let hVals = vec4f(q[0].height, q[1].height, q[2].height, q[3].height);

    let s = max(sVals, vec4f(0.001));

    let distX = vec4f(f.x, 1.0 - f.x, f.x, 1.0 - f.x);
    let distY = vec4f(f.y, f.y, 1.0 - f.y, 1.0 - f.y);

    let axisX = clamp((0.5 - distX) / s + 0.5, vec4f(0.0), vec4f(1.0));
    let axisY = clamp((0.5 - distY) / s + 0.5, vec4f(0.0), vec4f(1.0));

    var w = (axisX * axisY) / s;

    let invF = 1.0 - f;
    let wSpatial = vec4f(
        invF.x * invF.y, // TL
        f.x * invF.y,    // TR
        invF.x * f.y,    // BL
        f.x * f.y        // BR
    );

    w *= (1.0 + (hVals + sVals * 2.0) * 5.0) * wSpatial;

    var totalColor = vec4f(0.0);
    var totalWeight = 0.0;

    if (w.x > 0.0001) {
        totalColor += sampleTile(fetchTileUv(baseTile).xy, uvZ, lod, inset) * w.x;
        totalWeight += w.x;
    }
    if (w.y > 0.0001) {
        totalColor += sampleTile(fetchTileUv(baseTile + vec2f(1.0, 0.0)).xy, uvZ, lod, inset) * w.y;
        totalWeight += w.y;
    }
    if (w.z > 0.0001) {
        totalColor += sampleTile(fetchTileUv(baseTile + vec2f(0.0, 1.0)).xy, uvZ, lod, inset) * w.z;
        totalWeight += w.z;
    }
    if (w.w > 0.0001) {
        totalColor += sampleTile(fetchTileUv(baseTile + vec2f(1.0, 1.0)).xy, uvZ, lod, inset) * w.w;
        totalWeight += w.w;
    }

    return totalColor / max(totalWeight, 0.0001);
}

// one quadrant tile's contribution to the blended triplanar result; weight 0 means it was below threshold
fn blendedTriplanarCorner(td: TileData, distToCorner: vec2f, tilePos: vec2f, uvZ: vec2f, uvX: vec2f, uvY: vec2f, wAxis: vec3f, sideActive: bool, lod: f32, inset: f32) -> WeightedColor {
    let s = max(td.softness, 0.001);
    let axisWeight = clamp((vec2f(0.5) - distToCorner) / s + 0.5, vec2f(0.0), vec2f(1.0));
    var w = axisWeight.x * axisWeight.y * (1.0 / s);
    w *= (1.0 + (td.height + td.softness * 2.0) * 5.0);

    if (w <= 0.0001) { return WeightedColor(vec4f(0.0), 0.0); }

    let ts = fetchTileUv(tilePos).xy;
    var col = sampleTile(ts, uvZ, lod, inset);
    if (sideActive) {
        col = col * wAxis.z + sampleTile(ts, uvX, lod, inset) * wAxis.x + sampleTile(ts, uvY, lod, inset) * wAxis.y;
    }
    return WeightedColor(col * w, w);
}

fn getBlendedTriplanarColorNeighborhood(worldPos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32, inset: f32, nh: TileNeighborhood) -> vec4f {
    let n2 = normal * normal;
    var wAxis = n2 * n2;
    wAxis = wAxis / (wAxis.x + wAxis.y + wAxis.z);
    let sideActive = wAxis.x + wAxis.y > 0.003;

    let uvZ = fract(worldPos.xy);
    let uvX = vec2f(fract(worldPos.y), 1.0 - fract(worldPos.z * perspectiveScale));
    let uvY = vec2f(fract(worldPos.x), 1.0 - fract(worldPos.z * perspectiveScale));

    let gridPos = worldPos.xy - 0.5;
    let baseTile = floor(gridPos);
    let f = fract(gridPos);

    let q = selectQuadrant(nh, uvZ);

    let c0 = blendedTriplanarCorner(q[0], f,                            baseTile,                   uvZ, uvX, uvY, wAxis, sideActive, lod, inset);
    let c1 = blendedTriplanarCorner(q[1], vec2f(1.0 - f.x, f.y),        baseTile + vec2f(1.0, 0.0), uvZ, uvX, uvY, wAxis, sideActive, lod, inset);
    let c2 = blendedTriplanarCorner(q[2], vec2f(f.x, 1.0 - f.y),        baseTile + vec2f(0.0, 1.0), uvZ, uvX, uvY, wAxis, sideActive, lod, inset);
    let c3 = blendedTriplanarCorner(q[3], vec2f(1.0 - f.x, 1.0 - f.y),  baseTile + vec2f(1.0, 1.0), uvZ, uvX, uvY, wAxis, sideActive, lod, inset);

    let finalColor = c0.color + c1.color + c2.color + c3.color;
    let totalWeight = c0.weight + c1.weight + c2.weight + c3.weight;

    return finalColor / max(totalWeight, 0.0001);
}

fn getTriplanarColorNeighborhood(pos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32, inset: f32, nb: TileNeighborhood) -> vec4f {
    let n2 = normal * normal;
    var w = n2 * n2;
    w = w / (w.x + w.y + w.z);

    let tileID = fetchTileUv(floor(pos.xy));
    if (tileID.x < 0.0) { return vec4f(0.0); }

    let colZ = sampleTile(tileID, fract(pos.xy), lod, inset);
    if (w.x + w.y < 0.003) { return colZ; }

    let uvX = vec2f(fract(pos.y), 1.0 - fract(pos.z * perspectiveScale));
    let uvY = vec2f(fract(pos.x), 1.0 - fract(pos.z * perspectiveScale));

    let colX = sampleTile(tileID, uvX, lod, inset);
    let colY = sampleTile(tileID, uvY, lod, inset);

    return colX * w.x + colY * w.y + colZ * w.z;
}
