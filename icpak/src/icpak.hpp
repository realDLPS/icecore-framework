#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <map>

#include "icpak_openssl.hpp"

#define BLOCK_SIZE 1048576 // 1 MiB, will later be read from a config file.

struct icpak_asset_header
{
    std::uint8_t asset_type;

    std::uint8_t compression_type;
    std::uint8_t asset_flags; // Following flags are available 
    /*
    0: compressed 
    1: reserved
    2: reserved
    3: reserved
    4: reserved
    5: reserved
    6: reserved
    7: reserved
    */
    std::int32_t block; // Which block the asset is in
    std::int32_t offset; // Byte offset inside the block the asset is stored in
    
    std::int32_t compressed_size; // Byte size of the asset when compressed
    std::int32_t size; // Byte size of the asset after decompression
    
    sha256_hash compressed_hash;
    sha256_hash uncompressed_hash;
};

struct icpak_index
{
    std::int32_t block_count;
    std::vector<std::int32_t> block_sizes;
    std::vector<sha256_hash> block_hashes;
    std::map<icpak_uuid, icpak_asset_header> asset_map;

    // Calculates the offset to a block inside an icpak.
    bool CalculateBlockOffset(std::int32_t block, std::int32_t &result)
    {
        if(block > block_count) {   return false;   }

        std::int32_t offset = 0;

        for(int i = 0; i < block; i++)
        {
            offset += block_sizes[i] * BLOCK_SIZE;
        }

        result = offset;
        return true;
    }
};

struct icpak_toc
{
    
};


struct icpak_block
{
    std::vector<std::array<std::uint8_t, BLOCK_SIZE>> bytes;
};

struct icpak
{
    std::vector<icpak_block> blocks;
};

struct icpak_asset
{
    std::vector<std::uint8_t> bytes;
};