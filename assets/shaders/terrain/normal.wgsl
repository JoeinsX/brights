#include "tile.wgsl"
#include "heightfield.wgsl"

fn applyCornerGrad(uv: vec2f, corner: vec2f, diagH: f32, centerH: f32, softness: f32, edgeBlend: f32, bestDepth: ptr<function, f32>, grad: vec2f) -> vec2f {
    if (diagH >= centerH) { return grad; }
    let v = uv - corner;
    let d = softness - length(v);
    if (d <= 0.0) { return grad; }
    let cornerGrad = v * (centerH - diagH) * (6.0 * d / (softness * softness * softness));

    if (d > *bestDepth) {
        *bestDepth = d;
        return cornerGrad;
    }

    return mix(grad, cornerGrad, edgeBlend);
}

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

    let tX = ec.dX / softness;
    let tY = ec.dY / softness;
    let sX = smoothstep(0.0, 1.0, tX);
    let sY = smoothstep(0.0, 1.0, tY);
    let dsX = 6.0 * tX * (1.0 - tX) / softness;
    let dsY = 6.0 * tY * (1.0 - tY) / softness;
    let dropX = centerH - ec.hX;
    let dropY = centerH - ec.hY;
    let crossDrop = min(dropX, dropY);
    var bestDepth = length(vec2f(ec.dX, ec.dY));
    var grad = vec2f(
        gradXdir * dsX * (crossDrop * sY - dropX),
        gradYdir * dsY * (crossDrop * sX - dropY)
    );

    let sL = smoothstep(0.0, 1.0, ec.cuts.x / softness);
    let sR = smoothstep(0.0, 1.0, ec.cuts.y / softness);
    let sU = smoothstep(0.0, 1.0, ec.cuts.z / softness);
    let sD = smoothstep(0.0, 1.0, ec.cuts.w / softness);

    grad = applyCornerGrad(uv, vec2f(0.0, 0.0), nb.tiles[5].height, centerH, softness, sL * sU, &bestDepth, grad);
    grad = applyCornerGrad(uv, vec2f(1.0, 0.0), nb.tiles[3].height, centerH, softness, sR * sU, &bestDepth, grad);
    grad = applyCornerGrad(uv, vec2f(0.0, 1.0), nb.tiles[7].height, centerH, softness, sL * sD, &bestDepth, grad);
    grad = applyCornerGrad(uv, vec2f(1.0, 1.0), nb.tiles[1].height, centerH, softness, sR * sD, &bestDepth, grad);

    if (grad.x == 0.0 && grad.y == 0.0) { return vec3f(0.0, 0.0, 1.0); }

    let up = select(1.0, 0.0, isSharp);
    return normalize(vec3f(-grad.x, -grad.y, up));
}
