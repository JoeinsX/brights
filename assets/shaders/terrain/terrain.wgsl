struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

struct Uniforms {
    macroOffset: vec2i,
    offset: vec2f,
    resolution: vec2f,
    scale: f32,
    mapSizeTiles: u32,
    sphereMapScale: f32,
    chunkSize: u32,
    chunkOffset: vec2i,
    resolutionScale: vec2f,
    perspectiveStrength: f32,
    perspectiveScale: f32,
};

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_tilemap: array<u32>;
@group(0) @binding(2) var t_atlas: texture_2d<f32>;
@group(0) @binding(3) var s_atlas: sampler;
@group(0) @binding(4) var<storage, read> s_packed: array<u32>;
@group(0) @binding(5) var<storage, read> s_chunkRefMap: array<u32>;

#include "tile.wgsl"

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

#include "util.wgsl"

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

fn getTerrainColor(pos: vec2f, lod: f32) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let idTop = fetchTileUv(floor(pos)).xy;

    let uvTop = fract(pos);

    return textureSampleLevel(t_atlas, s_atlas, (idTop + uvTop) / atlasGridSize, lod);
}

fn getTileBlendWeight(pos: vec2f) -> f32 {
return pos.x*50+pos.y;
}

#include "normal.wgsl"

fn hash(p: vec2f) -> f32 {
    return fract(sin(dot(p, vec2f(12.9898, 78.233))) * 43758.5453);
}

fn getBlendedColorNeighborhood(
    worldPos: vec3f,
    lod: f32,
    nh: TileNeighborhood
) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let uvZ = fract(worldPos.xy);
    let gridPos = worldPos.xy - 0.5;
    let baseTile = floor(gridPos);

    let noiseScale = 1.0; // How fast the wiggle changes
    let noiseAmp = 0.05;  // How wide the wiggle is

    let warp = vec2f(
        sin(worldPos.y * noiseScale),
        cos(worldPos.x * noiseScale)
    ) * noiseAmp;

    let f = fract(gridPos);
    //let f = clamp(fract(gridPos) + warp, vec2f(0.0), vec2f(1.0));

    var s_vals: vec4f; // Softness for TL, TR, BL, BR
    var h_vals: vec4f; // Height for TL, TR, BL, BR

    let is_right = uvZ.x >= 0.5;
    let is_down  = uvZ.y >= 0.5;

    if (is_right) {
        if (is_down) {
            // Quadrant: Bottom-Right
            s_vals = vec4f(nh.tiles[0].softness, nh.tiles[2].softness, nh.tiles[8].softness, nh.tiles[1].softness);
            h_vals = vec4f(nh.tiles[0].height,   nh.tiles[2].height,   nh.tiles[8].height,   nh.tiles[1].height);
        } else {
            // Quadrant: Top-Right
            s_vals = vec4f(nh.tiles[4].softness, nh.tiles[3].softness, nh.tiles[0].softness, nh.tiles[2].softness);
            h_vals = vec4f(nh.tiles[4].height,   nh.tiles[3].height,   nh.tiles[0].height,   nh.tiles[2].height);
        }
    } else {
        if (is_down) {
            // Quadrant: Bottom-Left
            s_vals = vec4f(nh.tiles[6].softness, nh.tiles[0].softness, nh.tiles[7].softness, nh.tiles[8].softness);
            h_vals = vec4f(nh.tiles[6].height,   nh.tiles[0].height,   nh.tiles[7].height,   nh.tiles[8].height);
        } else {
            // Quadrant: Top-Left
            s_vals = vec4f(nh.tiles[5].softness, nh.tiles[4].softness, nh.tiles[6].softness, nh.tiles[0].softness);
            h_vals = vec4f(nh.tiles[5].height,   nh.tiles[4].height,   nh.tiles[6].height,   nh.tiles[0].height);
        }
    }

    let s = max(s_vals, vec4f(0.001));

    let dist_x = vec4f(f.x, 1.0 - f.x, f.x, 1.0 - f.x);
    let dist_y = vec4f(f.y, f.y, 1.0 - f.y, 1.0 - f.y);

    let axis_x = clamp((0.5 - dist_x) / s + 0.5, vec4f(0.0), vec4f(1.0));
    let axis_y = clamp((0.5 - dist_y) / s + 0.5, vec4f(0.0), vec4f(1.0));

    var w = (axis_x * axis_y) / s;

    let inv_f = 1.0 - f;
    let w_spatial = vec4f(
        inv_f.x * inv_f.y, // TL
        f.x * inv_f.y,     // TR
        inv_f.x * f.y,     // BL
        f.x * f.y          // BR
    );

    w *= (1.0 + (h_vals + s_vals * 2.0) * 5.0) * w_spatial;

    var totalColor = vec4f(0.0);
    var totalWeight = 0.0;

    // Tile 0: Top-Left
    if (w.x > 0.0001) {
        let ts = fetchTileUv(baseTile).xy; // + vec2(0,0)
        totalColor += textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod) * w.x;
        totalWeight += w.x;
    }

    // Tile 1: Top-Right
    if (w.y > 0.0001) {
        let ts = fetchTileUv(baseTile + vec2f(1.0, 0.0)).xy;
        totalColor += textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod) * w.y;
        totalWeight += w.y;
    }

    // Tile 2: Bottom-Left
    if (w.z > 0.0001) {
        let ts = fetchTileUv(baseTile + vec2f(0.0, 1.0)).xy;
        totalColor += textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod) * w.z;
        totalWeight += w.z;
    }

    // Tile 3: Bottom-Right
    if (w.w > 0.0001) {
        let ts = fetchTileUv(baseTile + vec2f(1.0, 1.0)).xy;
        totalColor += textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod) * w.w;
        totalWeight += w.w;
    }

    return totalColor / max(totalWeight, 0.0001);
}

fn getBlendedTriplanarColorNeighborhood(pos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32, nb: TileNeighborhood) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);
    var w = pow(abs(normal), vec3f(4.0));
    w = w / (w.x + w.y + w.z);

    let tileID = fetchTileUv(floor(pos.xy));
    if (tileID.x < 0.0) { return vec4f(0.0); }

    let uvZ = fract(pos.xy);
    let uvX = vec2f(fract(pos.y), 1.0 - fract(pos.z * perspectiveScale));
    let uvY = vec2f(fract(pos.x), 1.0 - fract(pos.z * perspectiveScale));

    let colZ = getBlendedColorNeighborhood(pos, lod, nb);
    let colX = textureSampleLevel(t_atlas, s_atlas, (tileID + uvX) / atlasGridSize, lod);
    let colY = textureSampleLevel(t_atlas, s_atlas, (tileID + uvY) / atlasGridSize, lod);

    return colX * w.x + colY * w.y + colZ * w.z;
}

fn getBlendedTriplanarColorNeighborhood1(
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
            let colZ = textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod);
            let colX = textureSampleLevel(t_atlas, s_atlas, (ts + uvX) / atlasGridSize, lod);
            let colY = textureSampleLevel(t_atlas, s_atlas, (ts + uvY) / atlasGridSize, lod);
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
            let colZ = textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod);
            let colX = textureSampleLevel(t_atlas, s_atlas, (ts + uvX) / atlasGridSize, lod);
            let colY = textureSampleLevel(t_atlas, s_atlas, (ts + uvY) / atlasGridSize, lod);
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
            let colZ = textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod);
            let colX = textureSampleLevel(t_atlas, s_atlas, (ts + uvX) / atlasGridSize, lod);
            let colY = textureSampleLevel(t_atlas, s_atlas, (ts + uvY) / atlasGridSize, lod);
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
            let colZ = textureSampleLevel(t_atlas, s_atlas, (ts + uvZ) / atlasGridSize, lod);
            let colX = textureSampleLevel(t_atlas, s_atlas, (ts + uvX) / atlasGridSize, lod);
            let colY = textureSampleLevel(t_atlas, s_atlas, (ts + uvY) / atlasGridSize, lod);
            finalColor += (colX * wAxis.x + colY * wAxis.y + colZ * wAxis.z) * w;
            totalWeight += w;
        }
    }

    return finalColor / max(totalWeight, 0.0001);
}

fn getTriplanarColorNeighborhood(pos: vec3f, normal: vec3f, perspectiveScale: f32, lod: f32, nb: TileNeighborhood) -> vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);
    var w = pow(abs(normal), vec3f(4.0));
    w = w / (w.x + w.y + w.z);

    let tileID = fetchTileUv(floor(pos.xy));
    if (tileID.x < 0.0) { return vec4f(0.0); }

    let uvZ = fract(pos.xy);
    let uvX = vec2f(fract(pos.y), 1.0 - fract(pos.z * perspectiveScale));
    let uvY = vec2f(fract(pos.x), 1.0 - fract(pos.z * perspectiveScale));

    let colZ = textureSampleLevel(t_atlas, s_atlas, (tileID + uvZ) / atlasGridSize, lod);
    let colX = textureSampleLevel(t_atlas, s_atlas, (tileID + uvX) / atlasGridSize, lod);
    let colY = textureSampleLevel(t_atlas, s_atlas, (tileID + uvY) / atlasGridSize, lod);

    return colX * w.x + colY * w.y + colZ * w.z;
}

const PI = 3.1415926535897932384626433832795;

const simpleModeScaleThreshold = 3.0;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let screenPos = in.uv * u_config.resolution;
    var baseWorldPos = (screenPos / u_config.scale) + u_config.offset;
    let resolutionScale = u_config.resolutionScale;
    let viewCenter = u_config.offset + (u_config.resolution / u_config.scale) * 0.5;

    let simpleModeActive = u_config.scale < 3.0;

    let simpleModeSmoothCoefficient = smoothstep(0.0, 1.0, u_config.scale - simpleModeScaleThreshold);

    let perspectiveStrength = u_config.perspectiveStrength * simpleModeSmoothCoefficient;
    let perspectiveScale = u_config.perspectiveScale;

    let viewVec = (baseWorldPos - viewCenter) * u_config.scale * perspectiveStrength / resolutionScale;

    let radialVector = baseWorldPos - viewCenter;
    let sphereRadius = f32(u_config.mapSizeTiles)/2.0;

    let p = radialVector / sphereRadius;
    let distSq = dot(p, p);

    if(distSq > 1.0)
    {
        discard;
    }

    let z = sqrt(1.0 - distSq);
    let sphereNormal = vec3f(p.x, p.y, z);

    let phi = atan2(sphereNormal.x, sphereNormal.z);
    let theta = asin(sphereNormal.y);

    let u = sphereNormal.x / (1.0 + sphereNormal.z)*0.5;
    let v = sphereNormal.y / (1.0 + sphereNormal.z)*0.5;

    baseWorldPos = viewCenter + vec2f(u, v) * f32(u_config.mapSizeTiles) * u_config.sphereMapScale;

    let up = vec3f(0.0, 1.0, 0.0);
    let tangent = normalize(cross(up, sphereNormal + vec3f(0.00001)));
    let bitangent = cross(sphereNormal, tangent);
    let tbn = mat3x3f(tangent, bitangent, sphereNormal);

    var finalColor = vec4f(0.0);

    var rayPos = vec3f(baseWorldPos, 2.0);

    let rotatedViewVec = normalize(tbn * vec3f(0.0, 0.0, -1.0)) * vec3f(viewVec, 1.0);

    let rayDir1 = normalize(vec3f(viewVec, -1.0));

    let rayDir2 = normalize(mix(rotatedViewVec, vec3f(0.0, 0.0, -1.0), distSq*distSq * 0.2f));

    let rayDir = normalize(mix(rayDir1, rayDir2, distSq/2.0));

    let gridStepDir = sign(rayDir.xy);
    let gridBorderOffset = step(vec2f(0.0), gridStepDir);

    var hit = false;

    var nh = fetchTileNeighborhood(rayPos.xy);

    if (u_config.scale < 3.0) {
        let h = getSmoothedHeightNeighborhood(rayPos.xy, nh);
        rayPos.z = h;
        hit = true;
    }
    else {
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
                if(borderTime.z <=0.0 && (tileSoftness <= 0.05 || tileData.skipRaymarching))
                {
                    hit = true;
                    break;
                }

                let prevRayPos = rayPos;
                rayPos += rayDir * (exitTime3 + 0.005);

                borderDistance = vec3f(gridBorder - rayPos.xy, tileMaxHeight - rayPos.z);
                borderTime = abs(borderDistance / rayDir);

                exitTime2 = min(borderTime.x, borderTime.y);
                exitTime3 = min(exitTime2, borderTime.z);

                if(tileSoftness <= 0.05 || tileData.skipRaymarching)
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

                    let steps = i32(sqrt(160.0 * max(0.0, 0.5 - tileSoftness)) * f32(nh.tiles[0].advancedRaymarching));

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
    }

    if(hit) {
        let lod = log(25.f/u_config.scale*min(u_config.resolutionScale.x, u_config.resolutionScale.y))-1.0;

        let localNormal = getAnalyticalNormalNeighborhood(rayPos.xy, nh);

        let rotatedNormal = normalize(tbn * localNormal);

        let f = fract(rayPos.xy);

        var albedo: vec4f;

        let blending = nh.tiles[0].blending;

        if(nh.tiles[0].triplanar)
        {
            if(blending)
            {
                albedo = getBlendedTriplanarColorNeighborhood1(rayPos, localNormal, perspectiveScale, lod, nh);
            }
            else
            {
                albedo = getTriplanarColorNeighborhood(rayPos, localNormal, perspectiveScale, lod, nh);
            }
        }
        else
        {
            if(blending)
            {
                albedo = getBlendedColorNeighborhood(rayPos, lod, nh);
            }
            else
            {
                albedo = getTerrainColor(rayPos.xy, lod);
            }
        }

        //let normalColor = vec4f((rotatedNormal + vec3f(1.0))/2.0, 1.0);

        let lightDir = normalize(vec3f(-0.5, -0.8, 1.0));
        let diff = max(dot(rotatedNormal, lightDir), 0.0);
        let ambient = 0.4;
        let lighting = min(1.0, ambient + diff);

        finalColor = albedo * lighting;
    } else {
        finalColor = vec4f(0.1, 0.1, 0.1, 1.0);
    }

    return finalColor;
}