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

fn getSmoothedHeightNeighborhood(worldPos: vec2f, nh: TileNeighborhood) -> f32 {
    let centerH = nh.tiles[0].height;
    if (centerH <= 0.01) { return 0.0; }

    let uv = fract(worldPos);
    let softness = nh.tiles[0].softness;
    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);

    if (all(edgeDists >= vec4f(softness))) { return centerH; }

    let ec = computeEdgeCuts(edgeDists, centerH, softness, nh);

    var dist = length(vec2f(ec.dX, ec.dY));
    var targetH = centerH;
    if (dist > 0.0001) {
        targetH = (ec.hX * ec.dX + ec.hY * ec.dY) / (ec.dX + ec.dY);
    }

    if (nh.tiles[5].height < centerH) {
        let d = softness - length(uv);
        if (d > dist) { dist = d; targetH = nh.tiles[5].height; }
    }
    if (nh.tiles[3].height < centerH) {
        let d = softness - length(uv - vec2f(1.0, 0.0));
        if (d > dist) { dist = d; targetH = nh.tiles[3].height; }
    }
    if (nh.tiles[7].height < centerH) {
        let d = softness - length(uv - vec2f(0.0, 1.0));
        if (d > dist) { dist = d; targetH = nh.tiles[7].height; }
    }
    if (nh.tiles[1].height < centerH) {
        let d = softness - length(uv - vec2f(1.0, 1.0));
        if (d > dist) { dist = d; targetH = nh.tiles[1].height; }
    }

    if (dist <= 0.0) { return centerH; }

    let t = clamp(dist / softness, 0.0, 1.0);
    let profile = smoothstep(1.0, 0.0, t);
    return mix(targetH, centerH, profile);
}
