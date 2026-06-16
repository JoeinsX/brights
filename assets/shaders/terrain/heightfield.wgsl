#include "tile.wgsl"

// neighbor index legend: 0:Center 1:(1,1) 2:(1,0) 3:(1,-1) 4:(0,-1) 5:(-1,-1) 6:(-1,0) 7:(-1,1) 8:(0,1)

struct EdgeCut {
    cuts: vec4f, // L, R, U, D penetration into the softened edge band
    dX: f32,
    dY: f32,
    hX: f32,
    hY: f32,
};

// shared height/normal setup: how far the sample has crossed into each lower
// neighbor's soft edge, and which neighbor heights drive the X/Y blend
fn computeEdgeCuts(edgeDists: vec4f, centerH: f32, softness: f32, nb: TileNeighborhood) -> EdgeCut {
    let hL = nb.tiles[6].height;
    let hR = nb.tiles[2].height;
    let hU = nb.tiles[4].height;
    let hD = nb.tiles[8].height;

    let isLower = vec4f(
        step(hL, centerH - 0.001),
        step(hR, centerH - 0.001),
        step(hU, centerH - 0.001),
        step(hD, centerH - 0.001)
    );

    let cuts = max(vec4f(0.0), softness - edgeDists) * isLower;
    let dX = max(cuts.x, cuts.y);
    let dY = max(cuts.z, cuts.w);
    let hX = select(select(centerH, hR, cuts.y > cuts.x), hL, cuts.x > cuts.y);
    let hY = select(select(centerH, hD, cuts.w > cuts.z), hU, cuts.z > cuts.w);

    return EdgeCut(cuts, dX, dY, hX, hY);
}

fn applyCornerHeight(uv: vec2f, corner: vec2f, diagH: f32, centerH: f32, softness: f32, edgeBlend: f32, bestDepth: ptr<function, f32>, height: f32) -> f32 {
    if (diagH >= centerH) { return height; }
    let d = softness - length(uv - corner);
    if (d <= 0.0) { return height; }
    let cornerH = mix(centerH, diagH, smoothstep(0.0, 1.0, d / softness));

    if (d > *bestDepth) {
        *bestDepth = d;
        return cornerH;
    }

    return mix(height, cornerH, edgeBlend);
}

fn getSmoothedHeightNeighborhood(worldPos: vec2f, nh: TileNeighborhood) -> f32 {
    let centerH = nh.tiles[0].height;
    if (centerH <= 0.01) { return 0.0; }

    let uv = fract(worldPos);
    let rawSoftness = nh.tiles[0].softness;
    let softness = select(rawSoftness, 0.02, rawSoftness < 0.001);
    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);

    if (all(edgeDists >= vec4f(softness))) { return centerH; }

    let ec = computeEdgeCuts(edgeDists, centerH, softness, nh);

    let sX = smoothstep(0.0, 1.0, ec.dX / softness);
    let sY = smoothstep(0.0, 1.0, ec.dY / softness);
    let dropX = centerH - ec.hX;
    let dropY = centerH - ec.hY;
    var bestDepth = length(vec2f(ec.dX, ec.dY));
    var height = centerH - dropX * sX - dropY * sY + min(dropX, dropY) * sX * sY;

    let sL = smoothstep(0.0, 1.0, ec.cuts.x / softness);
    let sR = smoothstep(0.0, 1.0, ec.cuts.y / softness);
    let sU = smoothstep(0.0, 1.0, ec.cuts.z / softness);
    let sD = smoothstep(0.0, 1.0, ec.cuts.w / softness);

    height = applyCornerHeight(uv, vec2f(0.0, 0.0), nh.tiles[5].height, centerH, softness, sL * sU, &bestDepth, height);
    height = applyCornerHeight(uv, vec2f(1.0, 0.0), nh.tiles[3].height, centerH, softness, sR * sU, &bestDepth, height);
    height = applyCornerHeight(uv, vec2f(0.0, 1.0), nh.tiles[7].height, centerH, softness, sL * sD, &bestDepth, height);
    height = applyCornerHeight(uv, vec2f(1.0, 1.0), nh.tiles[1].height, centerH, softness, sR * sD, &bestDepth, height);

    return height;
}
