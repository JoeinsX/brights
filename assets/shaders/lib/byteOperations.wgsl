fn extractByte(word: u32, byteIdx: u32) -> u32 {
    return (word >> (byteIdx * 8u)) & 0xFFu;
}

fn extractBits(word: u32, offset: u32, count: u32) -> u32 {
    return (word >> offset) & ((1u << count) - 1u);
}
