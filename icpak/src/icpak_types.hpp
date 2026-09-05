#include <array>
#include <vector>
#include <algorithm>
#include <ranges>
#include <iterator>
#include <stdexcept>
#include <map>

#include <cstdint>
#include <cstring>

#if defined(BUILD_TEST)
#include "../../doctest/doctest/doctest.h"
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1048576 // 1 MiB, you may specify another size by defining yourself.
#endif

typedef std::uint8_t byte;
typedef std::uint32_t u32;
typedef std::uint64_t u64;
typedef std::array<byte, sizeof(u32)> u32bytes;
typedef std::uint8_t icpak_asset_type;

#pragma region Helpers
// Copies bytes from a std::vector<byte> to either std::vector or std::array
// Updates the used bytes variable.
template <typename Dest>
void CopyBytes(const std::vector<byte>& bytes, u32& used_bytes, u32 copied_byte_count, Dest& target)
{
    std::copy_n(bytes.begin() + used_bytes, copied_byte_count, target.begin());
    used_bytes += copied_byte_count;
}
#pragma endregion

#pragma region Conversions
// Big endian
inline u32bytes ToByte(u32 a)
{
    std::array<byte, 4> ret_val;
    ret_val[0] = (byte)(a >> 24);
    ret_val[1] = (byte)(a >> 16);
    ret_val[2] = (byte)(a >> 8);
    ret_val[3] = (byte)a;
    
    return ret_val;
}
// Big endian
inline u32 FromByte(u32bytes bytes)
{
    u32 ret_val;
    ret_val = (((u32)bytes[0]) << 24) | (((u32)bytes[1]) << 16) | (((u32)bytes[2]) << 8) | ((u32)bytes[3]);
    return ret_val;
}

#if defined(BUILD_TEST)
TEST_CASE("Testing byte conversions") {
    u32 original = 123;

    auto bytes = ToByte(original);
    u32 converted = FromByte(bytes);

    CHECK(original == converted);
}
#endif

#pragma endregion

// Data-type that can be serialized and deserialized to/from bytes.
struct Serializable
{
    virtual std::vector<byte> Serialize() = 0;
    virtual void Deserialize(std::vector<byte> &bytes) = 0;
};

#pragma region SHA256 hash
// Size of SHA256 hash in bytes
static constexpr u32 sha256_hash_size = 32;
struct sha256_hash : virtual Serializable
{
    std::array<byte, sha256_hash_size> hash_bytes = {0};

    sha256_hash(){};
    sha256_hash(std::array<byte, sha256_hash_size> hash)
    {
        hash_bytes = hash;
    }
    static const uint32_t byte_size()
    {
        return sha256_hash_size;
    }
    std::vector<byte> Serialize()
    {
        return std::vector<byte>(hash_bytes.begin(), hash_bytes.end());
    }
    void Deserialize(std::vector<byte> &bytes)
    {
        if(bytes.size() != sha256_hash_size)
        {
            throw(std::length_error("Incorrect amount of bytes supplied to dezerialize a sha256:hash"));
        }

        std::copy_n(bytes.begin(), sha256_hash_size, hash_bytes.begin());
    }
};
inline const bool operator==(const sha256_hash &lhs, const sha256_hash &rhs)
{
    return std::ranges::equal(lhs.hash_bytes, rhs.hash_bytes);
}
#if defined(BUILD_TEST)
TEST_CASE("Testing hash comparison") {
    
    sha256_hash hash1;
    sha256_hash hash2;

    for(u32 i = 0; i < sha256_hash_size; ++i)
    {
        hash1.hash_bytes[i] = i;
        hash2.hash_bytes[i] = i;
    }

    CHECK(hash1 == hash2);
}
TEST_CASE("Testing hash serialization") {
    
    sha256_hash hash1;

    for(u32 i = 0; i < sha256_hash_size; ++i)
    {
        hash1.hash_bytes[i] = i;
    }

    auto bytes = hash1.Serialize();

    sha256_hash hash2;
    hash2.Deserialize(bytes);

    CHECK(hash1 == hash2);
}
#endif

#pragma endregion





#pragma region UUID
// Size of a ICPAK UUID in bytes
static constexpr u32 icpak_uuid_size = 16;
typedef std::array<byte, icpak_uuid_size> icpak_uuid;

inline const bool operator==(const icpak_uuid &lhs, const icpak_uuid &rhs)
{
    return std::ranges::equal(lhs, rhs);
}

#if defined(BUILD_TEST)
TEST_CASE("Checking UUID comparison") {
    icpak_uuid uuid1;
    icpak_uuid uuid2;

    for(u32 i = 0; i < icpak_uuid_size; ++i)
    {
        uuid1[i] = i;
        uuid2[i] = i;
    }

    CHECK(uuid1 == uuid2);
}
#endif

#pragma endregion




// These types are required when loaded by the game
#pragma region Runtime 

namespace icpak {
enum compression_type : byte
{
    UNCOMPRESSED = 0
};
}

#pragma region Asset header
struct icpak_asset_header : virtual Serializable
{
    icpak_asset_header(){};

    icpak_asset_type asset_type = 0; // Optional meta data that the loader (the programmer) may use.
    icpak::compression_type compression_type = icpak::UNCOMPRESSED;
    byte asset_flags = 0; // One byte reserved for flag usage. Reservations may be lifted later on.
    /*
    0: reserved 
    1: reserved
    2: reserved
    3: reserved
    4: reserved
    5: reserved
    6: reserved
    7: reserved
    */
    u32 block = 0; // Which block the asset is in
    u32 offset = 0; // Byte offset inside the block the asset is stored in
    
    u32 compressed_size = 0; // Byte size of the asset when compressed
    u32 size = 0; // Byte size of the asset after decompression
    
    sha256_hash compressed_hash;
    sha256_hash uncompressed_hash;

    static const uint32_t byte_size()
    {
        return (sizeof(asset_type)+
            sizeof(compression_type)+
            sizeof(asset_flags)+
            sizeof(block)+
            sizeof(offset)+
            sizeof(compressed_size)+
            sizeof(size)+
            2 * sha256_hash_size);
    }

    std::vector<byte> Serialize()
    {
        std::vector<byte> ret_val;
        ret_val.reserve(
            byte_size()
        );

        ret_val.push_back(asset_type);

        ret_val.push_back(compression_type);

        ret_val.push_back(asset_flags);

        auto block_bytes = ToByte(block);
        ret_val.insert(ret_val.end(), block_bytes.begin(), block_bytes.end());

        auto offset_bytes = ToByte(offset);
        ret_val.insert(ret_val.end(), offset_bytes.begin(), offset_bytes.end());

        auto compressed_size_bytes = ToByte(compressed_size);
        ret_val.insert(ret_val.end(), compressed_size_bytes.begin(), compressed_size_bytes.end());

        auto size_bytes = ToByte(size);
        ret_val.insert(ret_val.end(), size_bytes.begin(), size_bytes.end());

        auto compressed_hash_bytes = compressed_hash.Serialize();
        ret_val.insert(ret_val.end(), compressed_hash_bytes.begin(), compressed_hash_bytes.end());

        auto uncompressed_hash_bytes = uncompressed_hash.Serialize();
        ret_val.insert(ret_val.end(), uncompressed_hash_bytes.begin(), uncompressed_hash_bytes.end());

        return ret_val;
    }
    void Deserialize(std::vector<byte> &bytes)
    {
        u32 used_bytes = 0;

        asset_type = bytes[used_bytes];
        used_bytes += sizeof(asset_type);

        compression_type = (icpak::compression_type)bytes[used_bytes];
        used_bytes += sizeof(compression_type);

        asset_flags = bytes[used_bytes];
        used_bytes += sizeof(asset_flags);

        u32bytes block_bytes;
        std::copy(bytes.begin() + used_bytes, bytes.begin() + used_bytes + sizeof(block), block_bytes.begin());
        block = FromByte(block_bytes);
        used_bytes += sizeof(block);

        u32bytes offset_bytes;
        std::copy(bytes.begin() + used_bytes, bytes.begin() + used_bytes + sizeof(offset), offset_bytes.begin());
        offset = FromByte(offset_bytes);
        used_bytes += sizeof(offset);

        u32bytes compressed_size_bytes;
        std::copy(bytes.begin() + used_bytes, bytes.begin() + used_bytes + sizeof(compressed_size), compressed_size_bytes.begin());
        compressed_size = FromByte(compressed_size_bytes);
        used_bytes += sizeof(compressed_size);

        u32bytes size_bytes;
        std::copy(bytes.begin() + used_bytes, bytes.begin() + used_bytes + sizeof(size), size_bytes.begin());
        size = FromByte(size_bytes);
        used_bytes += sizeof(size);

        std::vector<byte> compressed_hash_bytes;
        compressed_hash_bytes.resize(sha256_hash_size);
        std::copy(bytes.begin() + used_bytes, bytes.begin() + used_bytes + sha256_hash_size, compressed_hash_bytes.begin());
        compressed_hash.Deserialize(compressed_hash_bytes);
        used_bytes += sha256_hash_size;

        std::vector<byte> uncompressed_hash_bytes;
        uncompressed_hash_bytes.resize(sha256_hash_size);
        std::copy(bytes.begin() + used_bytes, bytes.begin() + used_bytes + sha256_hash_size, uncompressed_hash_bytes.begin());
        uncompressed_hash.Deserialize(uncompressed_hash_bytes);
        used_bytes += sha256_hash_size;

        return;
    }
};
inline const bool operator==(const icpak_asset_header &lhs, const icpak_asset_header &rhs)
{
    if(lhs.asset_type != rhs.asset_type) {return false;}
    if(lhs.compression_type != rhs.compression_type) {return false;}
    if(lhs.asset_flags != rhs.asset_flags) {return false;}
    if(lhs.block != rhs.block) {return false;}
    if(lhs.offset != rhs.offset) {return false;}
    if(lhs.compressed_size != rhs.compressed_size) {return false;}
    if(lhs.size != rhs.size) {return false;}
    if(!(lhs.compressed_hash == rhs.compressed_hash)) {return false;} // !(==) instead of != to not have to define !=
    if(!(lhs.uncompressed_hash == rhs.uncompressed_hash)) {return false;}

    return true;
}
#if defined(BUILD_TEST)
TEST_CASE("Testing icpak_asset_header comparison") {
    icpak_asset_header ast1 = icpak_asset_header();
    icpak_asset_header ast2 = icpak_asset_header();

    CHECK(ast1 == ast2);
}
TEST_CASE("Testing icpak_asset_header serialization") {
    icpak_asset_header original = icpak_asset_header();
    original.asset_type = 3;
    original.asset_flags = 2;
    original.compressed_hash.hash_bytes = {1};
    original.uncompressed_hash.hash_bytes = {2};

    auto bytes = original.Serialize();
    icpak_asset_header converted = icpak_asset_header();
    converted.Deserialize(bytes);

    CHECK(original == converted);
}
#endif
#pragma endregion





#pragma region PAK index
struct icpak_index : Serializable
{
    u32 block_count;
    std::vector<u32> block_sizes; // A multiplier of BLOCK_SIZE
    std::vector<sha256_hash> block_hashes;
    std::map<icpak_uuid, icpak_asset_header> asset_map;

    // Calculates the offset to a block inside an icpak.
    bool CalculateBlockOffset(u32 block, u64 &result, u32 block_size = BLOCK_SIZE)
    {
        if(block > block_count) {   return false;   }

        u64 offset = 0;

        for(u32 i = 0; i < block; i++)
        {
            offset += (u64)(block_sizes[i] * block_size);
        }

        result = offset;
        return true;
    }

    virtual std::vector<byte> Serialize()
    {
        std::vector<byte> ret_val;
        ret_val.reserve(
            sizeof(block_count)+
            (sizeof(std::uint32_t) * block_sizes.size())+
            (sha256_hash_size * block_hashes.size())+
            ((icpak_uuid_size + icpak_asset_header::byte_size()) * asset_map.size())
        );

        u32bytes block_count_bytes = ToByte(block_count);
        ret_val.insert(ret_val.end(), block_count_bytes.begin(), block_count_bytes.end());

        auto asset_mapping = asset_map.begin();

        for (u32 i = 0; i < block_count; ++i)
        {
            u32bytes block_size_bytes = ToByte(block_sizes[i]);
            ret_val.insert(ret_val.end(), block_size_bytes.begin(), block_size_bytes.end());

            auto block_hash_bytes = block_hashes[i].Serialize();
            ret_val.insert(ret_val.end(), block_hash_bytes.begin(), block_hash_bytes.end());

            ret_val.insert(ret_val.end(), asset_mapping->first.begin(), asset_mapping->first.end()); // UUID
            auto asset_header_bytes = asset_mapping->second.Serialize(); // Asset header
            ret_val.insert(ret_val.end(), asset_header_bytes.begin(), asset_header_bytes.end());
            std::next(asset_mapping, 1);
        }

        return ret_val;
    }
    virtual void Deserialize(std::vector<byte> &bytes)
    {
        u32 used_bytes = 0;

        u32bytes block_count_bytes;
        CopyBytes(bytes, used_bytes, sizeof(u32), block_count_bytes);
        block_count = FromByte(block_count_bytes);

        for (u32 i = 0; i < block_count; i++)
        {
            
        }
        
    }
};
#pragma endregion
#pragma endregion