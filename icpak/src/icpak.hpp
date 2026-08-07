#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <openssl/sha.h>
#include <openssl/evp.h>

#include "icfw_hashing.hpp"

#define BLOCK_SIZE 1048576 // 1 MiB, will later be read from a config file.

struct icpak_asset_header
{
    std::uint8_t asset_type = 0;

    std::uint8_t compression_type = 0;
    std::uint8_t asset_flags = 0; // Following flags are available 
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
    std::int32_t offset = 0; // Byte offset inside the block the asset is stored in
    
    std::int32_t compressed_size = 0; // Byte size of the asset when compressed
    std::int32_t size = 0; // Byte size of the asset after decompression

    std::array<std::uint8_t, 32> compressed_hash;
    std::array<std::uint8_t, 32> uncompressed_hash;
};

struct icpak_block
{
    std::vector<std::array<std::uint8_t, BLOCK_SIZE>> bytes;
};

struct icpak_asset
{
    std::vector<std::uint8_t> bytes;
};