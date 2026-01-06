fn getAnalyticalNormalNeighborhood(worldPos: vec2f, nb: TileNeighborhood) -> vec3f {
    let centerH = nb.tiles[0].height;
    if (centerH <= 0.01) { return vec3f(0.0, 0.0, 1.0); }

    let uv = fract(worldPos);
    let rawSoftness = nb.tiles[0].softness;
    let isSharp = rawSoftness < 0.001;
    let detectionSoftness = select(rawSoftness, 0.02, isSharp);

    // Neighbor Mapping based on clockwise indices:
    // L:6, R:2, U:4, D:8, TL:5, TR:3, BL:7, BR:1
    let hL = nb.tiles[6].height;
    let hR = nb.tiles[2].height;
    let hU = nb.tiles[4].height;
    let hD = nb.tiles[8].height;

    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);
    let isLower = vec4f(
        step(hL, centerH - 0.001),
        step(hR, centerH - 0.001),
        step(hU, centerH - 0.001),
        step(hD, centerH - 0.001)
    );

    let cuts = max(vec4f(0.0), detectionSoftness - edgeDists) * isLower;
    let dX = max(cuts.x, cuts.y);
    let dY = max(cuts.z, cuts.w);

    let hX = select(select(centerH, hR, cuts.y > cuts.x), hL, cuts.x > cuts.y);
    let hY = select(select(centerH, hD, cuts.w > cuts.z), hU, cuts.z > cuts.w);

    let gradX_dir = select(select(0.0, 1.0, cuts.y > cuts.x), -1.0, cuts.x > cuts.y);
    let gradY_dir = select(select(0.0, 1.0, cuts.w > cuts.z), -1.0, cuts.z > cuts.w);

    var dist = length(vec2f(dX, dY));
    var targetH = centerH;
    var distGradient = vec2f(0.0);

    if (dist > 0.0001) {
        targetH = (hX * dX + hY * dY) / (dX + dY);
        distGradient = vec2f(dX * gradX_dir, dY * gradY_dir) / dist;
    }

    if (nb.tiles[5].height < centerH) {
        let v = uv - vec2f(0.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = nb.tiles[5].height;
            distGradient = -normalize(v);
        }
    }

    if (nb.tiles[3].height < centerH) {
        let v = uv - vec2f(1.0, 0.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = nb.tiles[3].height;
            distGradient = -normalize(v);
        }
    }

    if (nb.tiles[7].height < centerH) {
        let v = uv - vec2f(0.0, 1.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = nb.tiles[7].height;
            distGradient = -normalize(v);
        }
    }

    if (nb.tiles[1].height < centerH) {
        let v = uv - vec2f(1.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = nb.tiles[1].height;
            distGradient = -normalize(v);
        }
    }

    if (dist <= 0.0) { return vec3f(0.0, 0.0, 1.0); }
    if (isSharp) { return vec3f(distGradient, 0.0); }

    let t = clamp(dist / detectionSoftness, 0.0, 1.0);
    let dProfile_dt = -6.0 * t * (1.0 - t);
    let slopeFactor = (centerH - targetH) * dProfile_dt * (1.0 / detectionSoftness);
    let gradH = distGradient * slopeFactor;

    return normalize(vec3f(-gradH.x, -gradH.y, 1.0));
}