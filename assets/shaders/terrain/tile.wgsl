 struct TileData {
    height: f32,
    softness: f32,
    skipRaymarching: bool,
    advancedRaymarching: bool,
    blending: bool,
    triplanar: bool
};

struct TileNeighborhood {
    tiles: array<TileData, 9> //0 is center, then (+1, +1), then clockwise
};

fn fetchTileUv(pos: vec2f) -> vec2f {
    let mapSizeTiles = u32(u_config.mapSizeTiles);

    let absPos = vec2i(floor(pos)) + u_config.macroOffset;

    if (absPos.x < 0 || absPos.x >= i32(mapSizeTiles) || absPos.y < 0 || absPos.y >= i32(mapSizeTiles)) {
        return vec2f(-1.0);
    }

    let idx = getTileIdx(pos);

    let word = s_tilemap[idx / 4u];
    let shift = (idx % 4u) * 8u;
    let byte = (word >> shift) & 0xFFu;

    return vec2f(f32(byte >> 4u), f32(byte & 0x0Fu));
}

fn getTileIdx(pos: vec2f) -> u32
{
    let mapSizeTiles = u32(u_config.mapSizeTiles);

    let absPos = vec2u(vec2i(floor(pos)) + u_config.macroOffset);

    var chunkPos = vec2i(vec2i(absPos/u_config.chunkSize)) + u_config.chunkOffset;
    let mapSizeChunks = u_config.mapSizeTiles/u_config.chunkSize;
    chunkPos = vec2i(chunkPos.x & i32(mapSizeChunks-1), chunkPos.y & i32(mapSizeChunks-1));
    let tilePos = absPos%u_config.chunkSize;

    let chunkIdx = u32(chunkPos.y)*mapSizeChunks+u32(chunkPos.x);

    let chunkOffset = chunkIdx * u_config.chunkSize * u_config.chunkSize;//todo decide what to do with s_chunkRefMap[chunkIdx];

    let idx = chunkOffset + u32(tilePos.y) * u_config.chunkSize + u32(tilePos.x);
    return idx;
}

fn unpackTileData(word: u32, isIdxOdd: bool) -> TileData
{
    let val = select(word & 0xFFFFu, word >> 16u, isIdxOdd);
    let h = f32((val >> 8u) & 0xFFu) / 127.5;
    let s = f32((val >> 4u) & 15u) / 15.0;
    let t = u32(val & 15u);
    //return TileData(h, s, false, false, false, false);
    return TileData(h, s, bool(t & 8u), bool(t & 4u), bool(t & 2u), bool(t & 1u));
}

fn fetchTileData(pos: vec2f) -> TileData {
    let mapSizeTiles = u_config.mapSizeTiles;
    let idx = getTileIdx(pos);

    // Each u32 contains 2 tiles (16 bits each)
    let word = s_packed[idx / 2u];
    return unpackTileData(word, (idx % 2u) == 1u);
}

fn fetchTwoTiles(pos: vec2f, isFirstTileOdd: bool) -> array<TileData, 2> {
    let mapSizeTiles = u_config.mapSizeTiles;
    let idx = getTileIdx(pos);//u32(i32(pos.y) + u_config.macroOffset.y) * u32(mapSizeTiles) + u32(i32(pos.x) + u_config.macroOffset.x);

    // Each u32 contains 2 tiles (16 bits each)
    let word = s_packed[idx / 2u];
    return array<TileData, 2>(unpackTileData(word, isFirstTileOdd), unpackTileData(word, !isFirstTileOdd));
}


fn fetchTileNeighborhood(pos: vec2f) -> TileNeighborhood {
    var nb: TileNeighborhood;
    let tilePos = floor(pos);

    let absTilePos = vec2i(tilePos) + u_config.macroOffset;
    let mapSizeTiles = u32(u_config.mapSizeTiles);
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