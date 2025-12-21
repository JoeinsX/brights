fn getTriplanarColor(pos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    var w = pow(abs(normal), vec3f(4.0));
    w = w / (w.x + w.y + w.z); // Normalize weights

    let lookupPos = pos - (normal * 0.005);
    let id = getTileData(floor(lookupPos.xy)).xy;

    if (id.x < 0.0) { return vec4f(0.0); }

    let uvZ = fract(pos.xy);

    let uvX = vec2f(fract(pos.y), 1.0 - fract(pos.z*perspectiveScale));
    let uvY = vec2f(fract(pos.x), 1.0 - fract(pos.z*perspectiveScale));

    let colZ = textureSampleLevel(t_atlas, s_atlas, (id + uvZ) / atlasGridSize, lod);
    let colX = textureSampleLevel(t_atlas, s_atlas, (id + uvX) / atlasGridSize, lod);
    let colY = textureSampleLevel(t_atlas, s_atlas, (id + uvY) / atlasGridSize, lod);

    return colX * w.x + colY * w.y + colZ * w.z;
}

fn getAnalyticalNormal(worldPos: vec2f) -> vec3f {
    let tilePos = floor(worldPos);
    let centerH = getRawHeight(tilePos);

    if (centerH <= 0.01) { return vec3f(0.0, 0.0, 1.0); }

    let uv = fract(worldPos);
    let rawSoftness = getSoftness(worldPos);

    let isSharp = rawSoftness < 0.001;
    let detectionSoftness = select(rawSoftness, 0.02, isSharp);

    let hL = getRawHeight(tilePos + vec2f(-1.0, 0.0));
    let hR = getRawHeight(tilePos + vec2f( 1.0, 0.0));
    let hU = getRawHeight(tilePos + vec2f( 0.0,-1.0));
    let hD = getRawHeight(tilePos + vec2f( 0.0, 1.0));

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

    // Top-Left
    if (getRawHeight(tilePos + vec2f(-1.0, -1.0)) < centerH) {
        let d = detectionSoftness - length(uv);
        if (d > dist) {
            dist = d;
            targetH = getRawHeight(tilePos + vec2f(-1.0, -1.0));
            distGradient = -normalize(uv);
        }
    }
    // Top-Right
    if (getRawHeight(tilePos + vec2f( 1.0, -1.0)) < centerH) {
        let v = uv - vec2f(1.0, 0.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = getRawHeight(tilePos + vec2f( 1.0, -1.0));
            distGradient = -normalize(v);
        }
    }
    // Bottom-Left
    if (getRawHeight(tilePos + vec2f(-1.0,  1.0)) < centerH) {
        let v = uv - vec2f(0.0, 1.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = getRawHeight(tilePos + vec2f(-1.0,  1.0));
            distGradient = -normalize(v);
        }
    }
    // Bottom-Right
    if (getRawHeight(tilePos + vec2f( 1.0,  1.0)) < centerH) {
        let v = uv - vec2f(1.0, 1.0);
        let d = detectionSoftness - length(v);
        if (d > dist) {
            dist = d;
            targetH = getRawHeight(tilePos + vec2f( 1.0,  1.0));
            distGradient = -normalize(v);
        }
    }

    if (dist <= 0.0) { return vec3f(0.0, 0.0, 1.0); }

    if (isSharp) {
        return vec3f(distGradient, 0.0);
    }

    let t = clamp(dist / detectionSoftness, 0.0, 1.0);

    // Derivative of smoothstep(1, 0, t) -> -6t(1-t)
    let dProfile_dt = -6.0 * t * (1.0 - t);

    let heightDiff = centerH - targetH;
    let slopeFactor = heightDiff * dProfile_dt * (1.0 / detectionSoftness);

    let gradH = distGradient * slopeFactor;
    return normalize(vec3f(-gradH.x, -gradH.y, 1.0));
}


fn getSmoothedHeight(worldPos: vec2f) -> f32 {
    let tilePos = floor(worldPos);
    let centerH = getRawHeight(tilePos);

    if (centerH <= 0.01) { return 0.0; }

    let uv = fract(worldPos);
    let softness = getSoftness(worldPos);

    let hL = getRawHeight(tilePos + vec2f(-1.0, 0.0));
    let hR = getRawHeight(tilePos + vec2f( 1.0, 0.0));
    let hU = getRawHeight(tilePos + vec2f( 0.0,-1.0));
    let hD = getRawHeight(tilePos + vec2f( 0.0, 1.0));

    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);

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

    var dist = length(vec2f(dX, dY));
    var targetH = centerH;

    if (dist > 0.0001) {
        targetH = (hX * dX + hY * dY) / (dX + dY);
    }

    if (dist >= softness) {
        return mix(targetH, centerH, 0.0);
    }

    if (getRawHeight(tilePos + vec2f(-1.0, -1.0)) < centerH) {
        let d = softness - length(uv);
        if (d > dist) { dist = d; targetH = getRawHeight(tilePos + vec2f(-1.0, -1.0)); }
    }
    // Top-Right
    if (getRawHeight(tilePos + vec2f( 1.0, -1.0)) < centerH) {
        let d = softness - length(uv - vec2f(1.0, 0.0));
        if (d > dist) { dist = d; targetH = getRawHeight(tilePos + vec2f( 1.0, -1.0)); }
    }
    // Bottom-Left
    if (getRawHeight(tilePos + vec2f(-1.0,  1.0)) < centerH) {
        let d = softness - length(uv - vec2f(0.0, 1.0));
        if (d > dist) { dist = d; targetH = getRawHeight(tilePos + vec2f(-1.0,  1.0)); }
    }
    // Bottom-Right
    if (getRawHeight(tilePos + vec2f( 1.0,  1.0)) < centerH) {
        let d = softness - length(uv - vec2f(1.0, 1.0));
        if (d > dist) { dist = d; targetH = getRawHeight(tilePos + vec2f( 1.0,  1.0)); }
    }
    if (dist <= 0.0) {
    return centerH;
    }

    let t = clamp(dist / softness, 0.0, 1.0);
    let profile = smoothstep(1.0, 0.0, t);
    return mix(targetH, centerH, profile);
}

fn getSoftness(pos: vec2f) -> f32 {
    return fetchTileData(pos).softness;

}

fn getComplexityTag(pos: vec2f) -> bool {
    return fetchTileData(pos).complexityTag;
}

fn getRawHeight(pos: vec2f) -> f32 {
    return fetchTileData(pos).height;
}
