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
    // Asset uuids mapped to asset hashes
    std::map<icpak_uuid, sha256_hash> asset_hashes;
    // Asset uuids mapped to asset block location
    std::map<icpak_uuid, std::uint32_t>asset_locations;
    // Vector of blocks containing vector of assets containing asset uuids and asset compressed sizes.
    std::vector<std::vector<std::tuple<icpak_uuid, std::uint32_t>>> block_contents;
    // Vector of paks containing vector of blocks.
    std::map<std::string, std::vector<std::uint32_t>> pak_contents;
};

struct ic_asset
{
    icpak_uuid uuid;
    std::uint8_t asset_type;
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
bool U32Write(std::uint32_t u32, FILE *file)
{
    auto bytes = ToByte(u32);
    return ValidatedByteWrite(bytes.data(), sizeof(std::uint32_t), file);
}
bool U32Read(std::uint32_t &u32, FILE* file)
{
    std::array<std::uint8_t, 4> buf;
    if(!ValidatedByteRead(buf.data(), sizeof(std::uint32_t), file))
    {
        return false;
    }
    u32 = FromByte(buf);
    return true;
}

// Tries to write a U32 and on fail closes file and returns false
#define U32WriteWithFail(u32, file) if(!U32Write(u32, file)){fclose(file);return false;}
// Tries to read a U32 and on fail closes file and returns false
// Creates variable with the name defined for u32.
#define U32ReadWithFail(u32, file) std::uint32_t u32; if(!U32Read(u32, file)){fclose(file);return false;}


bool WriteDepotManifest(depot_manifest manifest)
{
    std::string staging_file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/staging/manifest.icman";
    FILE* icmanfile = fopen64(staging_file_name.c_str(), "wb");
    if(!icmanfile) return false;

    ////////// Asset uuids, hashes & locations

    // Write how many assets are in the manifest
    U32WriteWithFail(manifest.asset_hashes.size(), icmanfile)
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
    U32WriteWithFail(manifest.block_contents.size(), icmanfile)
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

    ////////// Pak contents
    U32WriteWithFail(manifest.pak_contents.size(), icmanfile)
    for(const auto& [pak_name, blocks] : manifest.pak_contents)
    {   
        std::uint32_t pak_name_byte_size = pak_name.size() * sizeof(std::string::value_type); // Size requirement of the pak name 
        std::uint32_t block_count = blocks.size();
        std::uint32_t buf_size = 0;
        buf_size += sizeof(uint32_t) + pak_name_byte_size;
        buf_size += block_count * sizeof(uint32_t);

        std::uint8_t *buf = (std::uint8_t*)malloc(buf_size);
        std::uint32_t buf_offset = 0;

        auto pak_name_byte_size_bytes = ToByte(pak_name_byte_size);
        std::copy(pak_name_byte_size_bytes.begin(), pak_name_byte_size_bytes.end(), buf); // Write how much space the pak name takes
        buf_offset += sizeof(pak_name_byte_size);

        std::copy(pak_name.begin(), pak_name.end(), buf + buf_offset); // Write pak name to the buffer
        buf_offset += pak_name_byte_size;

        auto block_count_bytes = ToByte(block_count);
        std::copy(block_count_bytes.begin(), block_count_bytes.end(), buf + buf_offset); // Copy how many assets in the block
        buf_offset += sizeof(block_count);

        for(auto& block : blocks)
        {
            auto block_bytes = ToByte(block);
            std::copy(block_bytes.begin(), block_bytes.end(), buf + buf_offset);
            buf_offset += sizeof(block);
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
    U32ReadWithFail(asset_count, icmanfile)
    for(std::uint32_t i = 0; i < asset_count; ++i)
    {
        icpak_uuid uuid;
        sha256_hash hash;

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
        U32ReadWithFail(location, icmanfile)

        new_manifest.asset_hashes.emplace(uuid, hash);
        new_manifest.asset_locations.emplace(uuid, location);
    }

    ////////// Blocks
    U32ReadWithFail(block_count, icmanfile)

    for(std::uint32_t i = 0; i < block_count; ++i)
    {
        // Number of assets in this block
        U32ReadWithFail(block_asset_count, icmanfile)

        auto& assets = new_manifest.block_contents[i];
        assets.reserve(block_asset_count);

        for(std::uint32_t j = 0; j < block_asset_count; ++j)
        {
            icpak_uuid uuid;

            if(!ValidatedByteRead(uuid.data(), sizeof(icpak_uuid), icmanfile))
            {
                fclose(icmanfile);
                return false;
            }

            U32ReadWithFail(asset_size, icmanfile)

            assets.emplace_back(uuid, asset_size);
        }
    }

    ////////// Pak contents
    U32ReadWithFail(pak_count, icmanfile) // Read how many paks exist

    for(std::uint32_t i = 0; i < pak_count; ++i)
    {
        U32ReadWithFail(pak_name_size, icmanfile) // Read size of pak name

        std::string pak_name;
        if(!ValidatedByteRead(pak_name.data(), pak_name_size, icmanfile)) // Read pak name
        {
            fclose(icmanfile);
            return false;
        }

        U32ReadWithFail(block_count, icmanfile) // Read number of blocks in pak

        std::vector<std::uint32_t> blocks(block_count);

        for(std::uint32_t j = 0; j < block_count; ++j)
        {
            U32ReadWithFail(block, icmanfile) // Read block
            blocks[j] = block;
        }

        new_manifest.pak_contents[pak_name] = blocks;
    }

    fclose(icmanfile);

    manifest = new_manifest;
    return true;
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