//#define HEIGHT_SAMPLER
//#define DERIVATIVE_NORMALS

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

struct Uniforms {
    offset: vec2f,
    resolution: vec2f,
    scale: f32,
    mapSize: f32,
};

@group(0) @binding(0) var<uniform> u_config: Uniforms;
@group(0) @binding(1) var<storage, read> s_tilemap: array<vec4f>;
@group(0) @binding(2) var t_atlas: texture_2d<f32>;
@group(0) @binding(3) var s_atlas: sampler;
@group(0) @binding(4) var<storage, read> s_heights: array<f32>;
@group(0) @binding(5) var t_heights: texture_2d<f32>;
@group(0) @binding(6) var<storage, read> s_softness: array<f32>;


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

fn getSoftness(pos: vec2f) -> f32 {
    let mapSize = vec2f(u_config.mapSize, u_config.mapSize);
    if (pos.x < 0.0 || pos.x >= mapSize.x || pos.y < 0.0 || pos.y >= mapSize.y) {
        return -1.0;
    }
    let idx = u32(pos.y) * u32(mapSize.x) + u32(pos.x);
    return s_softness[idx];
}

fn getRawHeight(pos: vec2f) -> f32 {
    let mapSize = vec2f(u_config.mapSize, u_config.mapSize);
    if (pos.x < 0.0 || pos.x >= mapSize.x || pos.y < 0.0 || pos.y >= mapSize.y) {
        return -1.0;
    }
//#ifdef HEIGHT_SAMPLER
    //return textureLoad(t_heights, vec2u(pos), 0).r;
//#else
    let idx = u32(pos.y) * u32(mapSize.x) + u32(pos.x);
    return s_heights[idx];
//#endif
}

fn getTileData(pos: vec2f) -> vec4f {
    let mapSize = vec2f(u_config.mapSize, u_config.mapSize);
    if (pos.x < 0.0 || pos.x >= mapSize.x || pos.y < 0.0 || pos.y >= mapSize.y) {
        return vec4f(-1.0);
    }
    let idx = u32(pos.y) * u32(mapSize.x) + u32(pos.x);
    return s_tilemap[idx];
}

fn getSmoothedHeight(worldPos: vec2f) -> f32 {
    let tilePos = floor(worldPos);
    let centerH = getRawHeight(tilePos);

    // Optimization: If void, return 0 immediately
    if (centerH <= 0.01) { return 0.0; }

    let uv = fract(worldPos);
    let softness = getSoftness(worldPos); // Radius of the rounded edge

    // Neighbors
    let hL = getRawHeight(tilePos + vec2f(-1.0, 0.0));
    let hR = getRawHeight(tilePos + vec2f( 1.0, 0.0));
    let hU = getRawHeight(tilePos + vec2f( 0.0,-1.0));
    let hD = getRawHeight(tilePos + vec2f( 0.0, 1.0));

    // --- 1. ORTHOGONAL CHECKS ---
    // We track both the distance (dX/dY) and the height of the neighbor causing the cut (hX/hY).

    // Calculate distances to edges: Left, Right, Up, Down
    let edgeDists = vec4f(uv.x, 1.0 - uv.x, uv.y, 1.0 - uv.y);

    // Calculate potential cuts: max(0, softness - dist)
    // We only care if neighbor < centerH.
    // step(a, b) returns 1.0 if a < b.
    let isLower = vec4f(
        step(hL, centerH - 0.001),
        step(hR, centerH - 0.001),
        step(hU, centerH - 0.001),
        step(hD, centerH - 0.001)
    );

    // Calculate 'd' for all 4 sides at once
    let cuts = max(vec4f(0.0), softness - edgeDists) * isLower;

    // Find the strongest orthogonal cut
    // .xy is Left/Right, .zw is Up/Down
    let dX = max(cuts.x, cuts.y);
    let dY = max(cuts.z, cuts.w);

    // Determine hX and hY based on which side cut deeper
    let hX = select(select(centerH, hR, cuts.y > cuts.x), hL, cuts.x > cuts.y);
    let hY = select(select(centerH, hD, cuts.w > cuts.z), hU, cuts.z > cuts.w);

    var dist = length(vec2f(dX, dY));
    var targetH = centerH;

    if (dist > 0.0001) {
        targetH = (hX * dX + hY * dY) / (dX + dY);
    }

    if (dist >= softness) {
        return mix(targetH, centerH, 0.0); // Returns targetH
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

        // Final Profile
        if (dist <= 0.0) { return centerH; }

        let t = clamp(dist / softness, 0.0, 1.0);
        let profile = smoothstep(1.0, 0.0, t);
        return mix(targetH, centerH, profile);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let atlasGridSize = vec2f(16.0, 16.0);

    let screenPos = in.uv * u_config.resolution;
    let baseWorldPos = (screenPos / u_config.scale) + u_config.offset;
    let viewCenter = u_config.offset + (u_config.resolution / u_config.scale) * 0.5;

    let perspectiveStrength = 0.005;
    let viewVec = (baseWorldPos - viewCenter) * u_config.scale * perspectiveStrength;

    // --- RAYMARCHING SETUP ---
    let MAX_HEIGHT = 1.0;
    let STEPS = 16; // Primary steps

    var hitPos = baseWorldPos;
    var hitHeight = 0.0;
    var found = false;

    // Keep track of ray state for binary search
    var rayPos = baseWorldPos;
    var rayHeight = MAX_HEIGHT;
    let rayStepVec = -viewVec * (MAX_HEIGHT / f32(STEPS));
    let heightStep = MAX_HEIGHT / f32(STEPS);

    // 1. Linear Search
    for (var i = 0; i < STEPS; i++) {
        let terrainH = getSmoothedHeight(rayPos);

        if (terrainH >= rayHeight) {
            found = true;
            // We hit the surface, but we likely overshot.
            // The true intersection is between (rayPos - rayStepVec) and (rayPos)
            break;
        }

        rayPos -= rayStepVec;
        rayHeight -= heightStep;
    }

    // 2. Binary Search Refinement (Fixes artifacts)
    if (found) {
        // Start range: [beforeHit, currentHit]
        var startPos = rayPos + rayStepVec;
        var endPos = rayPos;
        var startH = rayHeight + heightStep;
        var endH = rayHeight;

        // 5 iterations is usually enough for pixel-perfect results
        for (var j = 0; j < 5; j++) {
            let midPos = mix(startPos, endPos, 0.5);
            let midRayH = mix(startH, endH, 0.5);
            let midTerrainH = getSmoothedHeight(midPos);

            if (midTerrainH >= midRayH) {
                // Hit is in the first half (closer to camera/start)
                endPos = midPos;
                endH = midRayH;
            } else {
                // Hit is in the second half
                startPos = midPos;
                startH = midRayH;
            }
        }
        hitPos = endPos;
        hitHeight = getSmoothedHeight(hitPos);
    }

    var finalColor = vec4f(0.05, 0.05, 0.08, 1.0);

    if (found) {
        let tileCoord = floor(hitPos);
        let tileData = getTileData(tileCoord);

        if (tileData.x >= 0.0) {
//#ifdef DERIVATIVE_NORMALS
            let dHdx = dpdx(hitHeight);
            let dHdy = dpdy(hitHeight);

            // Reconstruct normal from derivatives
            // The 0.005 factor controls the "sharpness" of the normal
            var normal = normalize(vec3f(-dHdx, -dHdy, 0.005));

//#else
            // Finite difference for smooth lighting on curves
            /*let eps = 0.005;
            let h_here = getSmoothedHeight(hitPos);
            let h_dx   = getSmoothedHeight(hitPos + vec2f(eps, 0.0));
            let h_dy   = getSmoothedHeight(hitPos + vec2f(0.0, eps));

            let v_dx = vec3f(eps, 0.0, h_dx - h_here);
            let v_dy = vec3f(0.0, eps, h_dy - h_here);

            var normal = normalize(cross(v_dx, v_dy));*/

//#endif
            if (normal.z < 0.0) { normal = -normal; }

            // --- LIGHTING ---
            let lightDir = normalize(vec3f(-0.5, -0.5, 1.0));
            let diff = max(dot(normal, lightDir), 0.0);

            // Add a bit of rim light/specular to accentuate the curve
            let viewDir = normalize(vec3f(0.0, 0.0, 1.0)); // Approximate top-down view
            let reflectDir = reflect(-lightDir, normal);
            let spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0) * 0.2;

            let ambient = 0.5;
            let light = min(ambient + diff * 0.7 + spec, 1.3);

            // --- UV MAPPING ---
            // Simple top-down projection.
            // For perfect mapping on vertical sides, you'd need Triplanar,
            // but for "soft" terrain, stretching is usually acceptable.
            let localUV = fract(hitPos);
            let terrainUV = (tileData.xy + localUV) / atlasGridSize;

            var texColor = textureSample(t_atlas, s_atlas, terrainUV);
            finalColor = texColor * vec4f(vec3f(light), 1.0);
        }
    }

    // Fog
    let toCenter = viewCenter - hitPos;
    let dist = length(toCenter);
    finalColor = mix(finalColor, vec4f(0.05, 0.05, 0.08, 1.0), smoothstep(0.0, 100.0, dist));

    return finalColor;
}