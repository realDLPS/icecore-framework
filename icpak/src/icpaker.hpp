#include "icpak.hpp"

#include <iostream>
#include <stdio.h>
#include <filesystem>
#include <tuple>
#include <algorithm>
#include <stdexcept>

#define TOC_VERSION 1
#define ICPAK_VERSION 1
#define ICPAK_INDEX_VERSION 1
#define ICPAK_ASSET_VERSION 1
#define ICPAK_ASSET_HEADER_VERSION 1

struct paker_ctx
{
    bool debug_mode = false;
    int block_size = 1048576;
    std::vector<int> dirty_blocks; // Blocks that need to be examined to see if they should be purged.
};

static paker_ctx ctx;

struct depot_manifest
{
    std::map<icpak_uuid, sha256_hash> asset_hashes;
    std::map<icpak_uuid, std::uint32_t>asset_locations;
    std::vector<std::vector<std::tuple<icpak_uuid, std::uint32_t>>> block_contents;
};

struct ic_asset
{
    icpak_uuid uuid;
    std::string path;
    icpak_compression_type compression_type;
};

// Big endian
std::array<std::uint8_t, 4> ToByte(std::uint32_t u32)
{
    std::array<std::uint8_t, 4> ret_val;
    ret_val[0] = (uint8_t)(u32 >> 24);
    ret_val[1] = (uint8_t)(u32 >> 16);
    ret_val[2] = (uint8_t)(u32 >> 8);
    ret_val[3] = (uint8_t)u32;
    
    return ret_val;
}
// Big endian
std::uint32_t FromByte(std::array<std::uint8_t, 4> u8)
{
    uint32_t ret_val;
    ret_val = (((uint32_t)u8[0]) << 24) | (((uint32_t)u8[1]) << 16) | (((uint32_t)u8[2]) << 8) | ((uint32_t)u8[3]);
    return ret_val;
}

// Validates that all bytes were written
bool ValidatedByteWrite(const void* buffer, size_t buffer_count, FILE *file)
{
    auto result = fwrite(buffer, sizeof(uint8_t), buffer_count, file);

    return result == buffer_count;
}
// Validates that all bytes were read
bool ValidatedByteRead(void *dst_buffer, size_t byte_count, FILE *file)
{
    auto result = fread(dst_buffer, sizeof(uint8_t), byte_count, file);

    return result == byte_count;
}

bool WriteDepotManifest(depot_manifest manifest)
{
    std::string staging_file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/staging/manifest.icman";
    FILE* icmanfile = fopen64(staging_file_name.c_str(), "wb");
    if(!icmanfile) return false;

    ////////// Asset uuids, hashes & locations

    // Write how many assets are in the manifest
    if(!ValidatedByteWrite(ToByte(manifest.asset_hashes.size()).data(), sizeof(uint32_t), icmanfile))
    {
        fclose(icmanfile);
        return false;
    }
    for(const auto& [uuid, hash] : manifest.asset_hashes) // Write asset data
    {
        std::uint8_t buf[sizeof(icpak_uuid)+sizeof(sha256_hash)+sizeof(std::uint32_t)];
        std::copy(uuid.begin(), uuid.end(), buf); // Copy uuid to buffer
        std::copy(hash.begin(), hash.end(), buf + sizeof(icpak_uuid)); // Copy hash to buffer
        
        auto loc_bytes = ToByte(manifest.asset_locations[uuid]); // Create 4 bytes out of uint32_t
        std::copy(loc_bytes.begin(), loc_bytes.end(), buf + sizeof(icpak_uuid)+sizeof(sha256_hash)); // Copy to buffer

        if(!ValidatedByteWrite(buf, sizeof(icpak_uuid)+sizeof(sha256_hash)+sizeof(std::uint32_t), icmanfile)) // Write to file
        {
            fclose(icmanfile);
            return false;
        }
    }

    ////////// Block contents

    // Write how many blocks are in the manifest
    if(!ValidatedByteWrite(ToByte(manifest.block_contents.size()).data(), sizeof(uint32_t), icmanfile))
    {
        fclose(icmanfile);
        return false;
    }
    for(std::uint32_t i; i < manifest.block_contents.size(); ++i) // Write block data
    {
        const auto &assets = manifest.block_contents[i];
        std::uint32_t buf_size = sizeof(uint32_t);/*Amount of assets in block*/
        std::uint32_t asset_count = assets.size();
        buf_size += asset_count * (sizeof(icpak_uuid) + sizeof(uint32_t));

        std::uint8_t *buf = (std::uint8_t*)malloc(buf_size);
        std::uint32_t buf_offset = 0;

        auto asset_count_bytes = ToByte(asset_count);
        std::copy(asset_count_bytes.begin(), asset_count_bytes.end(), buf + buf_offset); // Copy how many assets in the block
        buf_offset += sizeof(asset_count);

        for(auto& asset : assets)
        {
            std::copy(get<0>(asset).begin(), get<0>(asset).end(), buf + buf_offset); // Copy asset uuid
            buf_offset += sizeof(icpak_uuid);

            auto asset_size_bytes = ToByte(get<1>(asset));
            std::copy(asset_size_bytes.begin(), asset_size_bytes.end(), buf + buf_offset); // Copy asset size
            buf_offset += sizeof(uint32_t);
        }

        if(!ValidatedByteWrite(buf, buf_size, icmanfile))
        {
            fclose(icmanfile);
            return false;
        }
        free(buf);
    }

    fclose(icmanfile);

    std::string file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/manifest.icman";
    if(!std::filesystem::copy_file(staging_file_name, file_name, std::filesystem::copy_options::overwrite_existing))
    {
        return false;
    }

    return true;
}

bool ReadDepotManifest(depot_manifest &manifest)
{
    depot_manifest new_manifest;

    std::string file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/manifest.icman";

    FILE* icmanfile = fopen64(file_name.c_str(), "rb");
    if(!icmanfile) return false;

    ////////// Assets
    std::array<uint8_t, 4> asset_count_buf;
    if(!ValidatedByteRead(asset_count_buf.data(), sizeof(uint32_t), icmanfile))
    {
        fclose(icmanfile);
        return false;
    }
    auto asset_count = FromByte(asset_count_buf);
    
    for(std::uint32_t i = 0; i < asset_count; ++i)
    {
        icpak_uuid uuid;
        sha256_hash hash;
        std::array<std::uint8_t, 4> location_bytes;

        // Read uuid
        if(!ValidatedByteRead(uuid.data(), sizeof(icpak_uuid), icmanfile))
        {
            fclose(icmanfile);
            return false;
        }

        // Read hash
        if(!ValidatedByteRead(hash.data(), sizeof(sha256_hash), icmanfile))
        {
            fclose(icmanfile);
            return false;
        }

        // Read asset location
        if(!ValidatedByteRead(location_bytes.data(), location_bytes.size(), icmanfile))
        {
            fclose(icmanfile);
            return false;
        }

        std::uint32_t location = FromByte(location_bytes);

        new_manifest.asset_hashes.emplace(uuid, hash);
        new_manifest.asset_locations.emplace(uuid, location);
    }

    ////////// Blocks
    std::array<uint8_t, 4> block_count_buf;
    if(!ValidatedByteRead(block_count_buf.data(), sizeof(uint32_t), icmanfile))
    {
        fclose(icmanfile);
        return false;
    }
    auto block_count = FromByte(block_count_buf);

    for(std::uint32_t i = 0; i < block_count; ++i)
    {
        // Number of assets in this block
        std::array<uint8_t, 4> block_asset_count_bytes;
        if(!ValidatedByteRead(block_asset_count_bytes.data(), sizeof(std::uint32_t), icmanfile))
        {
            fclose(icmanfile);
            return false;
        }
        std::uint32_t block_asset_count = FromByte(block_asset_count_bytes);

        auto& assets = new_manifest.block_contents[i];
        assets.reserve(block_asset_count);

        for(std::uint32_t j = 0; j < block_asset_count; ++j)
        {
            icpak_uuid uuid;
            std::array<std::uint8_t, 4> size_bytes;

            if(!ValidatedByteRead(uuid.data(), sizeof(icpak_uuid), icmanfile))
            {
                fclose(icmanfile);
                return false;
            }

            if(!ValidatedByteRead(size_bytes.data(), sizeof(std::uint32_t), icmanfile))
            {
                fclose(icmanfile);
                return false;
            }
            std::uint32_t asset_size = FromByte(size_bytes);

            assets.emplace_back(uuid, asset_size);
        }
    }

    fclose(icmanfile);

    manifest = new_manifest;
}

bool WritePak(std::string name, std::vector<int> blocks)
{
    std::string staging_file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/staging/paks/" + name + ".icpak";
    FILE* icpakfile = fopen64(staging_file_name.c_str(), "wb");
    if(!icpakfile) return false;

    for(auto block : blocks)
    {
        FILE* icblockfile = fopen((std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/blocks/" + std::to_string(block) + ".icblock").c_str(), "rb");
        if(!icblockfile) return false;
        
        int read_size = ctx.block_size / 128;
        std::vector<std::uint8_t> read_buf(read_size);
        while(ValidatedByteRead(read_buf.data(), read_size, icblockfile))
        {
            if(!ValidatedByteWrite(read_buf.data(), read_size, icpakfile))
            {
                fclose(icblockfile);
                return false;
            }
        }
        fclose(icblockfile);
    }

    fclose(icpakfile);

    std::string file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/paks/" + name + ".icpak";
    if(!std::filesystem::copy_file(staging_file_name, file_name, std::filesystem::copy_options::overwrite_existing))
    {
        return false;
    }

    return true;
}

int paker()
{
    bool exit = true;

    while (!exit)
    {
        std::string command;
    }

    return 0;
}