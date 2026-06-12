#include "bindings.wgsl"
#include "constants.wgsl"
#include "lib/byteOperations.wgsl"
#include "tile.wgsl"

fn getTileIdx(pos: vec2f) -> u32 {
    let absPos = vec2u(vec2i(floor(pos)) + u_config.macroOffset);

    var chunkPos = vec2i(absPos / chunkSize) + u_config.chunkOffset;
    chunkPos &= vec2i(i32(chunksPerSide) - 1);
    let tilePos = absPos % chunkSize;

    let chunkIdx = u32(chunkPos.y) * chunksPerSide + u32(chunkPos.x);
    let chunkOffset = chunkIdx * chunkSize * chunkSize;

    return chunkOffset + tilePos.y * chunkSize + tilePos.x;
}

fn fetchTileUv(pos: vec2f) -> vec2f {
    let absPos = vec2i(floor(pos)) + u_config.macroOffset;

    if (absPos.x < 0 || absPos.x >= i32(mapSizeTiles) || absPos.y < 0 || absPos.y >= i32(mapSizeTiles)) {
        return vec2f(-1.0);
    }

    let idx = getTileIdx(pos);
    let byte = extractByte(s_tilemap[idx / 4u], idx % 4u);

    return vec2f(f32(byte >> 4u), f32(byte & 0x0Fu));
}

// each u32 in s_packed holds two tiles (16 bits each).
fn fetchTileData(pos: vec2f) -> TileData {
    let idx = getTileIdx(pos);
    let word = s_packed[idx / 2u];
    return unpackTileData(word, (idx % 2u) == 1u);
}

fn fetchTwoTiles(pos: vec2f, isFirstTileOdd: bool) -> array<TileData, 2> {
    let idx = getTileIdx(pos);
    let word = s_packed[idx / 2u];
    return array<TileData, 2>(unpackTileData(word, isFirstTileOdd), unpackTileData(word, !isFirstTileOdd));
}

fn fetchTileNeighborhood(pos: vec2f) -> TileNeighborhood {
    var nb: TileNeighborhood;
    let tilePos = floor(pos);

    let centerIdx = getTileIdx(pos);
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
