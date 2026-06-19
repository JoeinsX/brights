#include "tile.wgsl"

// neighbor index legend: 0:Center 1:(1,1) 2:(1,0) 3:(1,-1) 4:(0,-1) 5:(-1,-1) 6:(-1,0) 7:(-1,1) 8:(0,1)

fn tileSoftness(rawSoftness: f32) -> f32 {
    return min(select(rawSoftness, 0.02, rawSoftness < 0.001), 0.5);
}

fn reconstructHeight(uv: vec2f, nh: TileNeighborhood, softness: f32) -> f32 {
    let cH = nh.tiles[0].height;
    let hL = nh.tiles[6].height;
    let hR = nh.tiles[2].height;
    let hU = nh.tiles[4].height;
    let hD = nh.tiles[8].height;

    let eL = min(cH, hL);
    let eR = min(cH, hR);
    let eU = min(cH, hU);
    let eD = min(cH, hD);
    let cUL = min(min(cH, hL), min(hU, nh.tiles[5].height));
    let cUR = min(min(cH, hR), min(hU, nh.tiles[3].height));
    let cDL = min(min(cH, hL), min(hD, nh.tiles[7].height));
    let cDR = min(min(cH, hR), min(hD, nh.tiles[1].height));

    let aL = smoothstep(1.0, 0.0, clamp(uv.x / softness, 0.0, 1.0));
    let aR = smoothstep(1.0, 0.0, clamp((1.0 - uv.x) / softness, 0.0, 1.0));
    let aU = smoothstep(1.0, 0.0, clamp(uv.y / softness, 0.0, 1.0));
    let aD = smoothstep(1.0, 0.0, clamp((1.0 - uv.y) / softness, 0.0, 1.0));
    let mX = 1.0 - aL - aR;
    let mY = 1.0 - aU - aD;

    return cUL * aL * aU + cUR * aR * aU + cDL * aL * aD + cDR * aR * aD
         + eL * aL * mY + eR * aR * mY + eU * aU * mX + eD * aD * mX
         + cH * mX * mY;
}

fn getSmoothedHeightNeighborhood(worldPos: vec2f, nh: TileNeighborhood) -> f32 {
    let centerH = nh.tiles[0].height;
    if (centerH <= 0.01) { return 0.0; }

    let softness = tileSoftness(nh.tiles[0].softness);
    let uv = fract(worldPos);
    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);
    if (all(edgeDists >= vec4f(softness))) { return centerH; }

    return reconstructHeight(uv, nh, softness);
}
