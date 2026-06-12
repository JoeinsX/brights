const mapSizeTiles = 1024u;
const chunkSize = 32u;
const chunksPerSide = mapSizeTiles / chunkSize;

const atlasGridSize = vec2f(16.0, 16.0);

const simpleModeScaleThreshold = 3.0;
const maxViewLean = 1.0;

const raymarchBinarySearchSteps = 6;
