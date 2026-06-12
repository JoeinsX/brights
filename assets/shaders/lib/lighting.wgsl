fn applyDirectionalLight(albedo: vec3f, normal: vec3f, lightDir: vec3f, ambient: f32) -> vec3f {
    let diff = max(dot(normal, lightDir), 0.0);
    return albedo * min(1.0, ambient + diff);
}
