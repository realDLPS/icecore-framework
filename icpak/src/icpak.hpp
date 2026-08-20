#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <map>
#include <string>

#include "icpak_openssl.hpp"

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1048576 // 1 MiB, you may specify another size by defining yourself.
#endif

struct icpak_index
{
    std::int32_t block_count;
    std::vector<std::int32_t> block_sizes; // A multiplier of BLOCK_SIZE
    std::vector<sha256_hash> block_hashes;
    std::map<icpak_uuid, icpak_asset_header> asset_map;

    // Calculates the offset to a block inside an icpak.
    bool CalculateBlockOffset(std::int32_t block, std::int64_t &result, std::int32_t block_size = BLOCK_SIZE)
    {
        if(block > block_count) {   return false;   }

        std::int64_t offset = 0;

        for(int i = 0; i < block; i++)
        {
            offset += (std::int64_t)(block_sizes[i] * block_size);
        }

        result = offset;
        return true;
    }
};

struct icpak_toc
{
    std::map<icpak_uuid, std::string> asset_to_pak_map; // Contains information on which pak file contains the asset
    std::map<std::string, sha256_hash> icpak_index_hashes;

    std::int32_t toc_version;
    std::int32_t icpak_version;
    std::int32_t icpak_index_version;
    std::int32_t icpak_asset_version;
    std::int32_t icpak_asset_header_version;

    std::int32_t block_size;
};


struct icpak_block
{
    std::vector<std::array<std::uint8_t, BLOCK_SIZE>> bytes;
};

struct icpak_file
{
    std::vector<icpak_block> blocks;
};

struct icpak_asset
{
    std::vector<std::uint8_t> bytes;
};