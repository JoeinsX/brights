#include "tile.wgsl"
#include "heightfield.wgsl"

fn getAnalyticalNormalNeighborhood(worldPos: vec2f, nb: TileNeighborhood) -> vec3f {
    let centerH = nb.tiles[0].height;
    if (centerH <= 0.01) { return vec3f(0.0, 0.0, 1.0); }

    let uv = fract(worldPos);
    let rawSoftness = nb.tiles[0].softness;
    let isSharp = rawSoftness < 0.001;
    let softness = select(rawSoftness, 0.02, isSharp);
    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);

    if (all(edgeDists >= vec4f(softness))) { return vec3f(0.0, 0.0, 1.0); }

    let ec = computeEdgeCuts(edgeDists, centerH, softness, nb);

    let gradXdir = select(select(0.0, 1.0, ec.cuts.y > ec.cuts.x), -1.0, ec.cuts.x > ec.cuts.y);
    let gradYdir = select(select(0.0, 1.0, ec.cuts.w > ec.cuts.z), -1.0, ec.cuts.z > ec.cuts.w);

    var dist = length(vec2f(ec.dX, ec.dY));
    var targetH = centerH;
    var distGradient = vec2f(0.0);
    if (dist > 0.0001) {
        targetH = (ec.hX * ec.dX + ec.hY * ec.dY) / (ec.dX + ec.dY);
        distGradient = vec2f(ec.dX * gradXdir, ec.dY * gradYdir) / dist;
    }

    if (nb.tiles[5].height < centerH) {
        let v = uv - vec2f(0.0);
        let d = softness - length(v);
        if (d > dist) { dist = d; targetH = nb.tiles[5].height; distGradient = -normalize(v); }
    }
    if (nb.tiles[3].height < centerH) {
        let v = uv - vec2f(1.0, 0.0);
        let d = softness - length(v);
        if (d > dist) { dist = d; targetH = nb.tiles[3].height; distGradient = -normalize(v); }
    }
    if (nb.tiles[7].height < centerH) {
        let v = uv - vec2f(0.0, 1.0);
        let d = softness - length(v);
        if (d > dist) { dist = d; targetH = nb.tiles[7].height; distGradient = -normalize(v); }
    }
    if (nb.tiles[1].height < centerH) {
        let v = uv - vec2f(1.0);
        let d = softness - length(v);
        if (d > dist) { dist = d; targetH = nb.tiles[1].height; distGradient = -normalize(v); }
    }

    if (dist <= 0.0) { return vec3f(0.0, 0.0, 1.0); }
    if (isSharp) { return vec3f(distGradient, 0.0); }

    let t = clamp(dist / softness, 0.0, 1.0);
    let dProfile_dt = -6.0 * t * (1.0 - t);
    let slopeFactor = (centerH - targetH) * dProfile_dt * (1.0 / softness);
    let gradH = distGradient * slopeFactor;

    return normalize(vec3f(-gradH.x, -gradH.y, 1.0));
}
