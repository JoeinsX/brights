#include "common/worldConstants.wgsl"
#include "lib/byteOperations.wgsl"

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

fn unpackTileData(word: u32, isIdxOdd: bool) -> TileData {
    let val = select(word & 0xFFFFu, word >> 16u, isIdxOdd);
    let h = f32(extractByte(val, 1u)) / 255.0 * maxTerrainHeight;
    let s = f32(extractBits(val, 4u, 4u)) / 15.0;
    let t = extractBits(val, 0u, 4u);
    return TileData(h, s, bool(t & 8u), bool(t & 4u), bool(t & 2u), bool(t & 1u));
}
