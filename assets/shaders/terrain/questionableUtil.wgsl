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

fn getTriplanarColor(pos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    var w = pow(abs(normal), vec3f(4.0));
    w = w / (w.x + w.y + w.z); // Normalize weights

    let lookupPos = pos - (normal * 0.005);
    let id = fetchTileUv(floor(lookupPos.xy)).xy;

    if (id.x < 0.0) { return vec4f(0.0); }

    let uvZ = fract(pos.xy);

    let uvX = vec2f(fract(pos.y), 1.0 - fract(pos.z*perspectiveScale));
    let uvY = vec2f(fract(pos.x), 1.0 - fract(pos.z*perspectiveScale));

    let colZ = textureSampleLevel(t_atlas, s_atlas, (id + uvZ) / atlasGridSize, lod);
    let colX = textureSampleLevel(t_atlas, s_atlas, (id + uvX) / atlasGridSize, lod);
    let colY = textureSampleLevel(t_atlas, s_atlas, (id + uvY) / atlasGridSize, lod);

    return colX * w.x + colY * w.y + colZ * w.z;
}


fn getBlendedColorNeighborhood2(
    worldPos: vec3f,
    lod: f32,
    nh: TileNeighborhood
) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let uv = fract(worldPos.xy);
    let dir = sign(uv-vec2f(0.5));

    var totalColor = (getTerrainColor(worldPos.xy, lod) +
    getTerrainColor(worldPos.xy + dir, lod) +
    getTerrainColor(worldPos.xy + vec2f(dir.x, 0.0), lod) +
    getTerrainColor(worldPos.xy + vec2f(0.0, dir.y), lod))/4.0;

    return totalColor;

}

/*fn getBlendedTriplanarColorNeighborhood(
    worldPos: vec3f,
    normal: vec3f,
    perspectiveScale: f32,
    lod: f32,
    nh: TileNeighborhood
) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    var wAxis = pow(abs(normal), vec3f(4.0));
    wAxis = wAxis / (wAxis.x + wAxis.y + wAxis.z);

    let uvZ = fract(worldPos.xy);
    let uvX = vec2f(fract(worldPos.y), 1.0 - fract(worldPos.z * perspectiveScale));
    let uvY = vec2f(fract(worldPos.x), 1.0 - fract(worldPos.z * perspectiveScale));

    let gridPos = worldPos.xy - 0.5;
    let baseTile = floor(gridPos);
    let f = fract(gridPos);

    let uv_center = fract(worldPos.xy);
    let is_right = uv_center.x >= 0.5;
    let is_down  = uv_center.y >= 0.5;

    var td0: TileData;
    var td1: TileData;
    var td2: TileData;
    var td3: TileData;

    if (is_right) {
        if (is_down) {
            td0 = nh.tiles[0];
            td1 = nh.tiles[2];
            td2 = nh.tiles[8];
            td3 = nh.tiles[1];
        } else {
            td0 = nh.tiles[4];
            td1 = nh.tiles[3];
            td2 = nh.tiles[0];
            td3 = nh.tiles[2];
        }
    } else {
        if (is_down) {
            td0 = nh.tiles[6];
            td1 = nh.tiles[0];
            td2 = nh.tiles[7];
            td3 = nh.tiles[8];
        } else {
            td0 = nh.tiles[5];
            td1 = nh.tiles[4];
            td2 = nh.tiles[6];
            td3 = nh.tiles[0];
        }
    }

    var totalWeight = 0.0;
    var finalColor = vec4f(0.0);
    {
        let tileData = td0;
        let s = max(tileData.softness, 0.001);
        let axisWeight = clamp((vec2f(0.5) - f) / s + 0.5, vec2f(0.0), vec2f(1.0));
        var w = axisWeight.x * axisWeight.y * (1.0 / s);
        w *= (1.0 + (tileData.height + tileData.softness * 2.0) * 5.0);

        if (w > 0.0001) {
            let ts = fetchTileUv(baseTile).xy;
            let colZ = textureSample(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize);
            let colX = textureSample(t_atlas, s_atlas, (ts + uvX) / atlasGridSize);
            let colY = textureSample(t_atlas, s_atlas, (ts + uvY) / atlasGridSize);
            finalColor += (colX * wAxis.x + colY * wAxis.y + colZ * wAxis.z) * w;
            totalWeight += w;
        }
    }

    {
        let tileData = td1;
        let s = max(tileData.softness, 0.001);
        let dist = vec2f(1.0 - f.x, f.y);
        let axisWeight = clamp((vec2f(0.5) - dist) / s + 0.5, vec2f(0.0), vec2f(1.0));
        var w = axisWeight.x * axisWeight.y * (1.0 / s);
        w *= (1.0 + (tileData.height + tileData.softness * 2.0) * 5.0);

        if (w > 0.0001) {
            let ts = fetchTileUv(baseTile + vec2f(1.0, 0.0)).xy;
            let colZ = textureSample(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize);
            let colX = textureSample(t_atlas, s_atlas, (ts + uvX) / atlasGridSize);
            let colY = textureSample(t_atlas, s_atlas, (ts + uvY) / atlasGridSize);
            finalColor += (colX * wAxis.x + colY * wAxis.y + colZ * wAxis.z) * w;
            totalWeight += w;
        }
    }

    {
        let tileData = td2;
        let s = max(tileData.softness, 0.001);
        let dist = vec2f(f.x, 1.0 - f.y);
        let axisWeight = clamp((vec2f(0.5) - dist) / s + 0.5, vec2f(0.0), vec2f(1.0));
        var w = axisWeight.x * axisWeight.y * (1.0 / s);
        w *= (1.0 + (tileData.height + tileData.softness * 2.0) * 5.0);

        if (w > 0.0001) {
            let ts = fetchTileUv(baseTile + vec2f(0.0, 1.0)).xy;
            let colZ = textureSample(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize);
            let colX = textureSample(t_atlas, s_atlas, (ts + uvX) / atlasGridSize);
            let colY = textureSample(t_atlas, s_atlas, (ts + uvY) / atlasGridSize);
            finalColor += (colX * wAxis.x + colY * wAxis.y + colZ * wAxis.z) * w;
            totalWeight += w;
        }
    }

    {
        let tileData = td3;
        let s = max(tileData.softness, 0.001);
        let dist = vec2f(1.0 - f.x, 1.0 - f.y);
        let axisWeight = clamp((vec2f(0.5) - dist) / s + 0.5, vec2f(0.0), vec2f(1.0));
        var w = axisWeight.x * axisWeight.y * (1.0 / s);
        w *= (1.0 + (tileData.height + tileData.softness * 2.0) * 5.0);

        if (w > 0.0001) {
            let ts = fetchTileUv(baseTile + vec2f(1.0, 1.0)).xy;
            let colZ = textureSample(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize);
            let colX = textureSample(t_atlas, s_atlas, (ts + uvX) / atlasGridSize);
            let colY = textureSample(t_atlas, s_atlas, (ts + uvY) / atlasGridSize);
            finalColor += (colX * wAxis.x + colY * wAxis.y + colZ * wAxis.z) * w;
            totalWeight += w;
        }
    }

    return finalColor / max(totalWeight, 0.0001);
}*/
