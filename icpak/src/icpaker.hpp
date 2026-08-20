#include "icpak.hpp"

#include <iostream>
#include <stdio.h>
#include <filesystem>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <cctype>

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
    std::string name;
    icpak_asset_type asset_type = ASSET_TYPE_UNSET;
    std::string path = "";
    icpak_compression_type compression_type = UNCOMPRESSED;
};







#pragma region File IO Helpers
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
#pragma endregion





#pragma region Prompts
bool PromptYesNo(std::string prompt, bool def_val, bool &ret_val)
{
    std::string input;
    std::string retry_input;
    bool exit = false;

    while(!exit)
    {
        std::cout << prompt + (def_val ? " [Y/n]: " : " [y/N]: ");
        std::getline(std::cin, input);

        if(input == "") {ret_val = def_val; return true;}
        std::transform(input.begin(), input.end(), input.begin(), ::tolower);

        if(input == "y" || input == "yes") {ret_val = true; return true;}
        if(input == "n" || input == "no") {ret_val = false; return true;}

        std::cout << "Invalid input, press enter to retry or type anything to exit: ";
        std::getline(std::cin, retry_input);
        if(retry_input == "") {exit = true;}
    }
    return false;
}

bool PromptNumber(std::string prompt, std::uint32_t min, std::uint32_t max, std::uint32_t &ret_val)
{
    std::uint32_t input;
    std::string retry_input;
    bool exit = false;

    while(!exit)
    {
        std::cout << prompt;
        if(std::cin >> input && input >= min && input <= max)
        {
            ret_val = input;
            return true;
        }
        std::cout << "Invalid input, press enter to retry or type anything to exit: ";
        std::getline(std::cin, retry_input);
        if(retry_input == "") {exit = true;}
    }
    return false;
}
#pragma endregion

bool WriteDepotManifest(const depot_manifest &manifest)
{
    std::string staging_file_name = std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/staging/manifest.icman";
    FILE* icmanfile = fopen(staging_file_name.c_str(), "wb");
    if(!icmanfile) return false;

    ////////// Asset uuids, hashes & locations

    U32WriteWithFail(manifest.asset_hashes.size(), icmanfile) // Write how many assets are in the manifest
    for(const auto& [uuid, hash] : manifest.asset_hashes) // Write asset data
    {
        std::uint8_t buf[sizeof(icpak_uuid)+sizeof(sha256_hash)+sizeof(std::uint32_t)];
        std::copy(uuid.begin(), uuid.end(), buf); // Copy uuid to buffer
        std::copy(hash.begin(), hash.end(), buf + sizeof(icpak_uuid)); // Copy hash to buffer
        
        auto loc_bytes = ToByte(manifest.asset_locations.at(uuid)); // Create 4 bytes out of uint32_t
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
    for(std::uint32_t i = 0; i < manifest.block_contents.size(); ++i) // Write block data
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
            free(buf);
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
            free(buf);
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

    FILE* icmanfile = fopen(file_name.c_str(), "rb");
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
        assets.resize(block_asset_count);

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
        pak_name.resize(pak_name_size);
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
    FILE* icpakfile = fopen(staging_file_name.c_str(), "wb");
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

bool WriteAsset(const ic_asset &asset)
{
    std::string staging_file_name = std::string(ctx.debug_mode ? "assets-debug/staging/" : "assets/staging/") + asset.name + ".icast";
    FILE* icastfile = fopen(staging_file_name.c_str(), "wb");
    if(!icastfile) return false;

    if(!ValidatedByteWrite(asset.uuid.data(), sizeof(icpak_uuid), icastfile))
    {
        fclose(icastfile);
        return false;
    }

    std::uint32_t name_byte_size = asset.name.size() * sizeof(std::string::value_type);
    U32WriteWithFail(name_byte_size, icastfile)
    if(!ValidatedByteWrite(asset.name.data(), name_byte_size, icastfile))
    {
        fclose(icastfile);
        return false;
    }

    if(!ValidatedByteWrite(&asset.asset_type, sizeof(std::uint8_t), icastfile))
    {
        fclose(icastfile);
        return false;
    }

    std::uint32_t path_byte_size = asset.path.size() * sizeof(std::string::value_type);
    U32WriteWithFail(path_byte_size, icastfile)
    if(!ValidatedByteWrite(asset.path.data(), path_byte_size, icastfile))
    {
        fclose(icastfile);
        return false;
    }

    if(!ValidatedByteWrite(&asset.compression_type, sizeof(std::uint8_t), icastfile))
    {
        fclose(icastfile);
        return false;
    }

    fclose(icastfile);

    std::string file_name = std::string(ctx.debug_mode ? "assets-debug/" : "assets/") + asset.name + ".icast";
    if(!std::filesystem::copy_file(staging_file_name, file_name, std::filesystem::copy_options::overwrite_existing))
    {
        return false;
    }
    return true;
}

bool DeleteAsset(const ic_asset &asset)
{
    std::string file_name = std::string(ctx.debug_mode ? "assets-debug/" : "assets/") + asset.name + ".icast";
    
    return std::filesystem::remove(file_name);
}

bool ReadAsset(const std::string &path, ic_asset &asset)
{
    ic_asset new_asset;

    FILE* icastfile = fopen(path.c_str(), "rb");
    if(!icastfile) return false;

    if(!ValidatedByteRead(new_asset.uuid.data(), sizeof(icpak_uuid), icastfile))
    {
        fclose(icastfile);
        return false;
    }

    U32ReadWithFail(name_size, icastfile)
    new_asset.name.resize(name_size);
    if(!ValidatedByteRead(new_asset.name.data(), name_size, icastfile))
    {
        fclose(icastfile);
        return false;
    }

    if(!ValidatedByteRead(&new_asset.asset_type, sizeof(std::uint8_t), icastfile))
    {
        fclose(icastfile);
        return false;
    }

    U32ReadWithFail(path_size, icastfile)
    new_asset.path.resize(path_size);
    if(!ValidatedByteRead(new_asset.path.data(), path_size, icastfile))
    {
        fclose(icastfile);
        return false;
    }

    if(!ValidatedByteRead(&new_asset.compression_type, sizeof(std::uint8_t), icastfile))
    {
        fclose(icastfile);
        return false;
    }

    fclose(icastfile);

    asset = new_asset;
    return true;
}

// Overrides edited_asset with asset when called, do no supply an important asset
bool EditAsset(const ic_asset &asset, ic_asset &edited_asset)
{
    edited_asset = asset;

    auto EditType = [&edited_asset]() 
    {
        std::string options = "";
        std::uint32_t max = 0;
        
        for(auto &[key, val] : icpak_asset_types)
        {
            options += key + "[" + std::to_string(val) + "] ";
            max = std::max(max, (std::uint32_t)val);
        }
        std::cout << options;

        std::uint32_t num;
        if(PromptNumber("[0-" + std::to_string(max) + "]: ", 0, max, num))
        {
            edited_asset.asset_type = (icpak_asset_type)num;
        }
        else
        {
            return false;
        }

        return true;
    };

    auto EditName = [&asset, &edited_asset]()
    {
        std::cout << "Enter new name: ";
        std::string input;
        std::cin.ignore();
        std::getline(std::cin, input);
        std::cout << "\n";

        edited_asset.name = input;

        if(WriteAsset(edited_asset))
        {
            std::cout << "Wrote asset with name name, deleting old asset";

            DeleteAsset(asset);
        }
        else
        {
            std::cout << "Failed to write edited asset";
            return false;
        }
        
        return true;
    };

    if(edited_asset.asset_type == 0)
    {
        std::cout << "Detected no asset type, please select asset type\n";

        return EditType();
    }

    std::cout << "Select property to edit\n";
    std::uint32_t num;
    if(PromptNumber("Name[0] Asset type[1] Path[2] Compression type[3] [0-3]: ", 0, 3, num))
    {
        if(num == 0)
        {
            return EditName();
        }
        else if(num == 1)
        {
            return EditType();
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool CreateAsset(const std::string &name)
{
    ic_asset asset;
    asset.uuid = icpak::generate_uuid();
    asset.name = name;

    ic_asset editing_asset;
    if(EditAsset(asset, editing_asset))
    {
        asset = editing_asset;
    }

    return WriteAsset(asset);
}

bool ProcessAssets()
{
    return false;
}

bool CreateDirectories()
{
    std::filesystem::create_directories("assets/staging/");
    std::filesystem::create_directories("assets-debug/staging/");

    std::filesystem::create_directories("depot/blocks/");
    std::filesystem::create_directories("depot/paks/");
    std::filesystem::create_directories("depot/staging/blocks/");
    std::filesystem::create_directories("depot/staging/paks/");

    std::filesystem::create_directories("depot-debug/blocks/");
    std::filesystem::create_directories("depot-debug/paks/");
    std::filesystem::create_directories("depot-debug/staging/blocks/");
    std::filesystem::create_directories("depot-debug/staging/paks/");

    return true;
}

int paker()
{
    CreateDirectories(); // Ensure all directories exist.
    bool exit = true;

    while (!exit)
    {
        std::string command;
    }
    ic_asset asset;
    if(ReadAsset("assets/Test.icast", asset))
    {
        ic_asset edited_asset;
        EditAsset(asset, edited_asset);
    }
    else
    {
        CreateAsset("Test");
    }
    

    return 0;
}