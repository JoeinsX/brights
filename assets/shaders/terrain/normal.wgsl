#include "tile.wgsl"
#include "heightfield.wgsl"

fn getAnalyticalNormalNeighborhood(worldPos: vec2f, nb: TileNeighborhood) -> vec3f {
    let centerH = nb.tiles[0].height;
    if (centerH <= 0.01) { return vec3f(0.0, 0.0, 1.0); }

    let softness = tileSoftness(nb.tiles[0].softness);
    let uv = fract(worldPos);
    let e = 0.04;

    let xp = min(uv.x + e, 1.0);
    let xm = max(uv.x - e, 0.0);
    let yp = min(uv.y + e, 1.0);
    let ym = max(uv.y - e, 0.0);
    let gx = (reconstructHeight(vec2f(xp, uv.y), nb, softness) - reconstructHeight(vec2f(xm, uv.y), nb, softness)) / (xp - xm);
    let gy = (reconstructHeight(vec2f(uv.x, yp), nb, softness) - reconstructHeight(vec2f(uv.x, ym), nb, softness)) / (yp - ym);

    return normalize(vec3f(-gx, -gy, 1.0));
}
