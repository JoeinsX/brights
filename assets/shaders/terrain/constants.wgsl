#include "common/worldConstants.wgsl"

const atlasGridSize = vec2f(16.0, 16.0);

const lightDir = vec3f(-0.5, -0.8, 1.0);
const ambientLight = 0.4;

const raymarchHardSoftnessThreshold = 0.05;
