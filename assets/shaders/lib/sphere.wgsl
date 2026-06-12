// lift a unit-disk point (|diskPos| <= 1) onto the front hemisphere
fn sphereNormalFromDisk(diskPos: vec2f) -> vec3f {
    return vec3f(diskPos, sqrt(1.0 - dot(diskPos, diskPos)));
}

// stereographic projection of a hemisphere normal back to disk uv
fn stereographicDiskUv(n: vec3f) -> vec2f {
    return vec2f(n.x, n.y) / (1.0 + n.z) * 0.5;
}

fn buildTbn(normal: vec3f, up: vec3f) -> mat3x3f {
    let tangent = normalize(cross(up, normal + vec3f(0.00001)));
    let bitangent = cross(normal, tangent);
    return mat3x3f(tangent, bitangent, normal);
}
