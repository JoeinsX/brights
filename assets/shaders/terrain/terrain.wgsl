struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

struct Uniforms {
    macroOffset: vec2i,
    offset: vec2f,
    resolution: vec2f,
    scale: f32,
    mapSize: f32,
    resolutionScale: vec2f,
    perspectiveStrength: f32,
    perspectiveScale: f32,
};

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_tilemap: array<u32>;
@group(0) @binding(2) var t_atlas: texture_2d<f32>;
@group(0) @binding(3) var s_atlas: sampler;
@group(0) @binding(4) var<storage, read> s_packed: array<u32>;

struct TileData {
    height: f32,
    softness: f32,
    complexityTag: bool
};

struct TileNeighborhood {
    tiles: array<TileData, 9> //0 is center, then (+1, +1), then clockwise
};

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
    var pos = array<vec2f, 6>(
        vec2f(-1.0, -1.0), vec2f( 1.0, -1.0), vec2f(-1.0,  1.0),
        vec2f(-1.0,  1.0), vec2f( 1.0, -1.0), vec2f( 1.0,  1.0)
    );
    var p = pos[in_vertex_index];
    var out: VertexOutput;
    out.position = vec4f(p, 0.0, 1.0);
    out.uv = vec2f(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
    return out;
}

fn unpackTileData(word: u32, isIdxOdd: bool) -> TileData
{
    let val = select(word & 0xFFFFu, word >> 16u, isIdxOdd);
    let h = f32((val >> 8u) & 0xFFu) / 127.5;
    let s = f32((val >> 1u) & 0x7Fu) / 127.0;
    let t = bool(val & 1u);
    return TileData(h, s, t);
}

fn fetchTileData(pos: vec2f) -> TileData {
    let mapSize = u_config.mapSize;
    let idx = u32(i32(pos.y) + u_config.macroOffset.y) * u32(mapSize) + u32(i32(pos.x) + u_config.macroOffset.x);

    // Each u32 contains 2 tiles (16 bits each)
    let word = s_packed[idx / 2u];
    return unpackTileData(word, (idx % 2u) == 1u);
}

fn fetchTwoTiles(pos: vec2f, isFirstTileOdd: bool) -> array<TileData, 2> {
    let mapSize = u_config.mapSize;
    let idx = u32(i32(pos.y) + u_config.macroOffset.y) * u32(mapSize) + u32(i32(pos.x) + u_config.macroOffset.x);

    // Each u32 contains 2 tiles (16 bits each)
    let word = s_packed[idx / 2u];
    return array<TileData, 2>(unpackTileData(word, isFirstTileOdd), unpackTileData(word, !isFirstTileOdd));
}


fn fetchTileNeighborhood(pos: vec2f) -> TileNeighborhood {
    var nb: TileNeighborhood;
    let tilePos = floor(pos);

    // FIX: Calculate absolute world position to determine packing parity
    let absTilePos = vec2i(tilePos) + u_config.macroOffset;
    let mapSize = u32(u_config.mapSize);
    let centerIdx = u32(absTilePos.y) * mapSize + u32(absTilePos.x);
    let isCenterOdd = (centerIdx % 2u) != 0u;

    if (isCenterOdd) {
        // If Center is Odd, it shares a word with the tile to its LEFT (Even)
        // Row Y (Center & Left)
        let rowY = fetchTwoTiles(tilePos, true);
        nb.tiles[0] = rowY[0]; // Center (0, 0)
        nb.tiles[6] = rowY[1]; // L (-1, 0)
        nb.tiles[2] = fetchTileData(tilePos + vec2f(1.0, 0.0)); // R (1, 0)

        // Row Y-1 (Up & Top-Left)
        let rowUp = fetchTwoTiles(tilePos + vec2f(0.0, -1.0), true);
        nb.tiles[4] = rowUp[0]; // U (0, -1)
        nb.tiles[5] = rowUp[1]; // TL (-1, -1)
        nb.tiles[3] = fetchTileData(tilePos + vec2f(1.0, -1.0)); // TR (1, -1)

        // Row Y+1 (Down & Bottom-Left)
        let rowDown = fetchTwoTiles(tilePos + vec2f(0.0, 1.0), true);
        nb.tiles[8] = rowDown[0]; // D (0, 1)
        nb.tiles[7] = rowDown[1]; // BL (-1, 1)
        nb.tiles[1] = fetchTileData(tilePos + vec2f(1.0, 1.0)); // BR (1, 1)
    } else {
        // If Center is Even, it shares a word with the tile to its RIGHT (Odd)
        // Row Y (Center & Right)
        let rowY = fetchTwoTiles(tilePos, false);
        nb.tiles[0] = rowY[0]; // Center (0, 0)
        nb.tiles[2] = rowY[1]; // R (1, 0)
        nb.tiles[6] = fetchTileData(tilePos + vec2f(-1.0, 0.0)); // L (-1, 0)

        // Row Y-1 (Up & Top-Right)
        let rowUp = fetchTwoTiles(tilePos + vec2f(0.0, -1.0), false);
        nb.tiles[4] = rowUp[0]; // U (0, -1)
        nb.tiles[3] = rowUp[1]; // TR (1, -1)
        nb.tiles[5] = fetchTileData(tilePos + vec2f(-1.0, -1.0)); // TL (-1, -1)

        // Row Y+1 (Down & Bottom-Right)
        let rowDown = fetchTwoTiles(tilePos + vec2f(0.0, 1.0), false);
        nb.tiles[8] = rowDown[0]; // D (0, 1)
        nb.tiles[1] = rowDown[1]; // BR (1, 1)
        nb.tiles[7] = fetchTileData(tilePos + vec2f(-1.0, 1.0)); // BL (-1, 1)
    }

    return nb;
}

fn getSmoothedHeightNeighborhood(worldPos: vec2f, nh: TileNeighborhood) -> f32 {
    let center = nh.tiles[0];
    let centerH = center.height;

    if (centerH <= 0.01) { return 0.0; }

    let uv = fract(worldPos);
    let softness = center.softness;

    // 0:Center, 1:(1,1), 2:(1,0), 3:(1,-1), 4:(0,-1), 5:(-1,-1), 6:(-1,0), 7:(-1,1), 8:(0,1)
    let hL = nh.tiles[6].height; // Left (-1, 0)
    let hR = nh.tiles[2].height; // Right (1, 0)
    let hU = nh.tiles[4].height; // Up (0, -1)
    let hD = nh.tiles[8].height; // Down (0, 1)

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

    // Top-Left (5): (-1, -1)
    if (nh.tiles[5].height < centerH) {
        let d = softness - length(uv);
        if (d > dist) { dist = d; targetH = nh.tiles[5].height; }
    }
    // Top-Right (3): (1, -1)
    if (nh.tiles[3].height < centerH) {
        let d = softness - length(uv - vec2f(1.0, 0.0));
        if (d > dist) { dist = d; targetH = nh.tiles[3].height; }
    }
    // Bottom-Left (7): (-1, 1)
    if (nh.tiles[7].height < centerH) {
        let d = softness - length(uv - vec2f(0.0, 1.0));
        if (d > dist) { dist = d; targetH = nh.tiles[7].height; }
    }
    // Bottom-Right (1): (1, 1)
    if (nh.tiles[1].height < centerH) {
        let d = softness - length(uv - vec2f(1.0, 1.0));
        if (d > dist) { dist = d; targetH = nh.tiles[1].height; }
    }

    if (dist <= 0.0) { return centerH; }

    let t = clamp(dist / softness, 0.0, 1.0);
    let profile = smoothstep(1.0, 0.0, t);
    return mix(targetH, centerH, profile);
}


fn getTileData(pos: vec2f) -> vec2f {
    let mapSize = u32(u_config.mapSize);

    let absPos = vec2i(floor(pos + 0.005)) + u_config.macroOffset;

    if (absPos.x < 0 || absPos.x >= i32(mapSize) || absPos.y < 0 || absPos.y >= i32(mapSize)) {
        return vec2f(-1.0);
    }

    let idx = u32(absPos.y) * mapSize + u32(absPos.x);
    let word = s_tilemap[idx / 4u];
    let shift = (idx % 4u) * 8u;
    let byte = (word >> shift) & 0xFFu;

    return vec2f(f32(byte >> 4u), f32(byte & 0x0Fu));
}

fn getTerrainColor(pos: vec2f, lod: f32) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let idTop = getTileData(floor(pos)).xy;

    let uvTop = fract(pos);

    return textureSampleLevel(t_atlas, s_atlas, (idTop + uvTop) / atlasGridSize, lod);
}

fn getTileBlendWeight(pos: vec2f) -> f32 {
return pos.x*50+pos.y;
}

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

fn getTriplanarColorNeighborhood(pos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32, nb: TileNeighborhood) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);
    var w = pow(abs(normal), vec3f(4.0));
    w = w / (w.x + w.y + w.z);

    let tileID = getTileData(floor(pos.xy));
    if (tileID.x < 0.0) { return vec4f(0.0); }

    let uvZ = fract(pos.xy);
    let uvX = vec2f(fract(pos.y), 1.0 - fract(pos.z * perspectiveScale));
    let uvY = vec2f(fract(pos.x), 1.0 - fract(pos.z * perspectiveScale));

    let colZ = textureSampleLevel(t_atlas, s_atlas, (tileID + uvZ) / atlasGridSize, lod);
    let colX = textureSampleLevel(t_atlas, s_atlas, (tileID + uvX) / atlasGridSize, lod);
    let colY = textureSampleLevel(t_atlas, s_atlas, (tileID + uvY) / atlasGridSize, lod);

    return colX * w.x + colY * w.y + colZ * w.z;
}

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

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let screenPos = in.uv * u_config.resolution;
    let baseWorldPos = (screenPos / u_config.scale) + u_config.offset;
    let resolutionScale = u_config.resolutionScale;
    let viewCenter = u_config.offset + (u_config.resolution / u_config.scale) * 0.5;

    let perspectiveStrength = u_config.perspectiveStrength;
    let perspectiveScale = u_config.perspectiveScale;

    let viewVec = (baseWorldPos - viewCenter) * u_config.scale * perspectiveStrength / resolutionScale;

    var finalColor = vec4f(0.0);

    var rayPos = vec3f(baseWorldPos, 2.0);
    let rayDir = normalize(vec3f(viewVec, -1.0));

    let gridStepDir = sign(rayDir.xy);
    let gridBorderOffset = step(vec2f(0.0), gridStepDir);

    var hit = false;

    var nh = fetchTileNeighborhood(rayPos.xy);

    for(var i=0; i<10; i+=1)
    {
        let tileData = nh.tiles[0];
        let tileMaxHeight = tileData.height;

        let tileSoftness = tileData.softness;

        var gridBorder = floor(rayPos.xy) + gridBorderOffset;

        var borderDistance = vec3f(gridBorder - rayPos.xy, min(0.0, tileMaxHeight - rayPos.z));

        var borderTime = borderDistance / rayDir;

        var exitTime2 = min(borderTime.x, borderTime.y);
        var exitTime3 = min(exitTime2, borderTime.z);

        if(borderTime.z <= exitTime2)
        {
            if(borderTime.z <=0.0 && tileSoftness <= 0.05)
            {
                hit = true;
                break;
            }

            let prevRayPos = rayPos;
            rayPos += rayDir * (exitTime3 + 0.005);

            //if(floor(prevRayPos).x != floor(rayPos).x || floor(prevRayPos).y != floor(rayPos).y)
            //{
                //return vec4f(1.0, 0.0, 0.0, 1.0);
            //}

            borderDistance = vec3f(gridBorder - rayPos.xy, tileMaxHeight - rayPos.z);
            borderTime = abs(borderDistance / rayDir);

            exitTime2 = min(borderTime.x, borderTime.y);
            exitTime3 = min(exitTime2, borderTime.z);

            if(tileSoftness <= 0.05)
            {
                hit = true;
                break;
            }
            else
            {
                let exitTime = exitTime2 - 0.005;
                var exitRayPos = rayPos + rayDir * exitTime;

                let enterHeight = getSmoothedHeightNeighborhood(rayPos.xy, nh);

                if(enterHeight>=rayPos.z)
                {
                    hit = true;
                    break;
                }

                let exitHeight = getSmoothedHeightNeighborhood(exitRayPos.xy, nh);

                let complexityTag = nh.tiles[0].complexityTag;

                let steps = i32(sqrt(160.0 * max(0.0, 0.5 - tileSoftness)) * f32(complexityTag));

                let stepSize = exitTime / f32(steps+1);
                var marchedT = 0.0;
                var foundHit = false;

                for(var s = 0; s < steps; s++) {
                    marchedT += stepSize;
                    let testPos = rayPos + rayDir * marchedT;
                    let testPosC = clamp(testPos, min(rayPos, exitRayPos), max(rayPos, exitRayPos));
                    let h = getSmoothedHeightNeighborhood(testPosC.xy, nh);

                    if(testPos.z <= h) {
                        exitRayPos = testPos;
                        foundHit = true;
                        break;
                    }
                }

                if(exitHeight < exitRayPos.z && !foundHit) //no intersection
                {
                    rayPos += rayDir * (exitTime2 + 0.005);
                    nh = fetchTileNeighborhood(rayPos.xy);
                }
                else
                {
                    var currentRayPos = (rayPos+exitRayPos)/2.0;
                    for(var j=0; j<10; j+=1)
                    {
                        let currentHeight = getSmoothedHeightNeighborhood(currentRayPos.xy, nh);
                        let prevCurrentRayPos = currentRayPos;
                        if(currentRayPos.z > currentHeight)
                        {
                            currentRayPos = (currentRayPos + exitRayPos)/2.0;
                            rayPos = prevCurrentRayPos;
                        }
                        else
                        {
                            currentRayPos = (rayPos + currentRayPos)/2.0;
                            exitRayPos = prevCurrentRayPos;
                        }
                    }
                    rayPos = currentRayPos;
                    hit = true;
                    break;
                }
            }
        }
        else {
            rayPos += rayDir * (exitTime2 + 0.005);
            nh = fetchTileNeighborhood(rayPos.xy);
        }
    }

    if(hit) {
        let ct = nh.tiles[0].complexityTag;
        let normal = getAnalyticalNormalNeighborhood(rayPos.xy, nh);
        let albedo = getTriplanarColorNeighborhood(rayPos, normal, perspectiveScale, log(25.f/u_config.scale*min(u_config.resolutionScale.x, u_config.resolutionScale.y))-1.0, nh);
        let normalColor = vec4f((normal + vec3f(1.0))/2.0, 1.0);
        let lightDir = normalize(vec3f(-0.5, -0.8, 1.0));
        let diff = max(dot(normal, lightDir), 0.0);
        let ambient = 0.4;
        let lighting = min(1.0, ambient + diff);

        finalColor = albedo * lighting;
            //finalColor = normalColor;
    } else {
        finalColor = vec4f(0.1, 0.1, 0.1, 1.0);
    }

    return finalColor;
}