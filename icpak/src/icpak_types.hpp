#include <array>
#include <vector>
#include <algorithm>
#include <ranges>
#include <iterator>
#include <stdexcept>
#include <map>
#include <span>
#include <limits>

#include <cstdint>
#include <cstring>

#if defined(BUILD_TEST)
#include "../../doctest/doctest/doctest.h"
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1048576 // 1 MiB, you may specify another size by defining yourself.
#endif

#ifndef ICPAK_MAX_BLOCK_COUNT
// With the default block size of 1 MiB this means that a PAK can hold 16 GiB. Though with variable sized blocks it could be higher.
// For example the maximum block size is effectively 4 GiB due to 32 bit offsets, so the pak could in theory hold 64 TiB, which is silly :D
//
// You may specify another count by defining this yourself.
#define ICPAK_MAX_BLOCK_COUNT 16384
#endif

#ifndef ICPAK_MAX_ASSET_COUNT
// No real logic to this, 8 388 608 just seemed like enough assets for a single pak.
//
// You may specify another count by defining this yourself.
#define ICPAK_MAX_ASSET_COUNT 8388608 
#endif

typedef std::uint8_t byte;
typedef std::uint32_t u32;
typedef std::uint64_t u64;
typedef std::array<byte, sizeof(u32)> u32bytes;
typedef std::uint8_t icpak_asset_type;
typedef std::span<const byte> byte_span;

#pragma region Helpers
// Returns false on overflow
static bool ValidatedAdd(size_t a, size_t b, size_t& result)
{
    if(b > std::numeric_limits<size_t>::max() - a)
    {
        return false;
    }
    result = a + b;
    return true;
}
// Returns false on overflow
static bool ValidatedMul(size_t a, size_t b, size_t& result)
{
    if(a == 0)
    {
        result = 0;
        return true;
    }
    if(b > std::numeric_limits<size_t>::max() / a)
    {
        return false;
    }
    result = a * b;
    return true;
}
// memcpy's sizeof(T) bytes from the provided bytes and
// updates the offset according to this.
template <typename T>
void ReadBytes(byte_span bytes, size_t& offset, T &target)
{
    if(offset > bytes.size())
    {
        throw std::overflow_error("Offset is outside of byte size");
        return;
    }
    if(sizeof(T) > bytes.size() - offset)
    {
        throw std::overflow_error("Offset is outside of byte size");
        return;
    }
    std::memcpy(&target, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);

    return;
}
byte_span SafeSubSpan(byte_span bytes, size_t offset, size_t count)
{
    if(offset > bytes.size())
    {
        throw std::overflow_error("Offset is outside of byte size");
        return byte_span();
    }
    if(count > bytes.size() - offset)
    {
        throw std::overflow_error("Offset is outside of byte size");
        return byte_span();
    }
    return bytes.subspan(offset, count);
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
    virtual std::vector<byte> Serialize() const = 0;
    virtual void Deserialize(std::span<const byte> bytes) = 0;
};
// Serializable data-type that has its size known at compile time
// Serializable "Size known" (SK)
struct Serializable_SK : Serializable
{
    static constexpr size_t byte_size = 0;
};


#pragma region SHA256 hash
// Size of SHA256 hash in bytes
static constexpr size_t sha256_hash_size = 32;
struct sha256_hash : virtual Serializable_SK
{
    std::array<byte, sha256_hash_size> data = {0};

    sha256_hash(){};
    sha256_hash(std::array<byte, sha256_hash_size> hash)
    {
        data = hash;
    }

    static constexpr size_t byte_size = sha256_hash_size;

    std::vector<byte> Serialize() const
    {
        return std::vector<byte>(data.begin(), data.end());
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t used = 0;
        ReadBytes<std::array<byte, sha256_hash_size>>(bytes, used, data);
    }
};
inline const bool operator==(const sha256_hash &lhs, const sha256_hash &rhs)
{
    return std::ranges::equal(lhs.data, rhs.data);
}
#if defined(BUILD_TEST)
TEST_CASE("Testing hash comparison #1") {
    
    sha256_hash hash1;
    sha256_hash hash2;

    for(u32 i = 0; i < sha256_hash_size; ++i)
    {
        hash1.hash_bytes[i] = i;
        hash2.hash_bytes[i] = i;
    }

    CHECK(hash1 == hash2);
}
TEST_CASE("Testing hash comparison #2") {
    
    sha256_hash hash1;
    sha256_hash hash2;

    for(u32 i = 0; i < sha256_hash_size; ++i)
    {
        hash1.hash_bytes[i] = i;
        hash2.hash_bytes[i] = sha256_hash_size - i;
    }

    CHECK(!(hash1 == hash2));
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
struct icpak_uuid : virtual Serializable_SK
{
    std::array<byte, icpak_uuid_size> data = {0};
    icpak_uuid(){};
    icpak_uuid(std::array<byte, icpak_uuid_size> uuid)
    {
        data = uuid;
    }

    static constexpr size_t byte_size = icpak_uuid_size;

    std::vector<byte> Serialize() const
    {
        return std::vector<byte>(data.begin(), data.end());
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t offset = 0;
        ReadBytes<std::array<byte, icpak_uuid_size>>(bytes, offset, data);
    }
};
inline const bool operator==(const icpak_uuid &lhs, const icpak_uuid &rhs)
{
    return std::ranges::equal(lhs.data, rhs.data);
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
struct icpak_asset_header : virtual Serializable_SK
{
    icpak_asset_header(){};

    icpak_asset_type asset_type = 0; // Optional meta data that the user (the game developer) may use.
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
    
    sha256_hash compressed_hash = sha256_hash();
    sha256_hash uncompressed_hash = sha256_hash();

    static constexpr size_t byte_size = (
        sizeof(asset_type)+
        sizeof(compression_type)+
        sizeof(asset_flags)+
        sizeof(block)+
        sizeof(offset)+
        sizeof(compressed_size)+
        sizeof(size)+
        2 * sha256_hash_size
    );

    std::vector<byte> Serialize() const
    {
        std::vector<byte> ret_val;
        ret_val.reserve(byte_size);

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
    void Deserialize(std::span<const byte> bytes)
    {
        if(bytes.size() != byte_size)
        {
            throw(std::out_of_range("Incorrect number of bytes supplied to deserialize asset header."));
            return;
        }
        size_t used_bytes = 0;

        asset_type = bytes[used_bytes];
        used_bytes += sizeof(asset_type);

        compression_type = (icpak::compression_type)bytes[used_bytes];
        used_bytes += sizeof(compression_type);

        asset_flags = bytes[used_bytes];
        used_bytes += sizeof(asset_flags);

        u32bytes block_bytes;
        ReadBytes<u32bytes>(bytes, used_bytes, block_bytes);
        block = FromByte(block_bytes);

        u32bytes offset_bytes;
        ReadBytes<u32bytes>(bytes, used_bytes, offset_bytes);
        offset = FromByte(offset_bytes);

        u32bytes compressed_size_bytes;
        ReadBytes<u32bytes>(bytes, used_bytes, compressed_size_bytes);
        compressed_size = FromByte(compressed_size_bytes);

        u32bytes size_bytes;
        ReadBytes<u32bytes>(bytes, used_bytes, size_bytes);
        size = FromByte(size_bytes);

        compressed_hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
        used_bytes += sha256_hash_size;

        uncompressed_hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
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
    std::vector<std::pair<u32, sha256_hash>> blocks; // u32 being block size, a multiplier of BLOCK_SIZE, sha256_hash being the block hash
    std::map<icpak_uuid, icpak_asset_header> asset_map;

    // Calculates the offset to a block inside an icpak.
    bool CalculateBlockOffset(u32 block, size_t &result, u32 block_size = BLOCK_SIZE)
    {
        if(block >= blocks.size()) {   return false;   }

        size_t offset = 0;

        for(u32 i = 0; i < block; ++i)
        {
            size_t cur = 0;
            if(!ValidatedMul((size_t)blocks[i].first, (size_t)block_size, cur))
            {
                return false;
            }

            size_t new_offset = 0;
            if(!ValidatedAdd(offset, cur, new_offset))
            {
                return false;
            }

            offset = new_offset;
        }

        result = offset;
        return true;
    }

    static size_t byte_size(u32 block_count, u32 asset_count)
    {
        const size_t block_entry_size = sizeof(u32) + sha256_hash::byte_size;
        const size_t asset_entry_size = icpak_uuid::byte_size + icpak_asset_header::byte_size;
        
        size_t blocks_total = 0;
        if(!ValidatedMul(block_entry_size, (size_t)block_count, blocks_total))
        {
            throw std::overflow_error("Block count is too large. icpak_index byte_size overflowed");
        }

        size_t assets_total = 0;
        if(!ValidatedMul(asset_entry_size, (size_t)asset_count, assets_total))
        {
            throw std::overflow_error("Asset count is too large. icpak_index byte_size overflowed");
        }

        // This is the byte size of the two count fields at the start.
        size_t header_size = 0;
        if(!ValidatedAdd(sizeof(u32), sizeof(u32), header_size))
        {
            throw std::overflow_error("Cannot compute byte size. icpak_index byte_size overflowed");
        }

        size_t partial_result = 0;
        if(!ValidatedAdd(header_size, blocks_total, partial_result))
        {
            throw std::overflow_error("Block count is too large. icpak_index byte_size overflowed");
        }

        size_t result = 0;
        if(!ValidatedAdd(partial_result, assets_total, result))
        {
            throw std::overflow_error("Asset count is too large. icpak_index byte_size overflowed");
        }

        return result;
    }

    std::vector<byte> Serialize() const
    {
        u32 block_count = (u32)blocks.size();
        u32 asset_count = (u32)asset_map.size();

        std::vector<byte> ret_val;
        ret_val.reserve(byte_size(block_count, asset_count));

        u32bytes block_count_bytes = ToByte(block_count);
        ret_val.insert(ret_val.end(), block_count_bytes.begin(), block_count_bytes.end());

        u32bytes asset_count_bytes = ToByte(asset_count);
        ret_val.insert(ret_val.end(), asset_count_bytes.begin(), asset_count_bytes.end());

        for(auto& block : blocks)
        {
            u32bytes block_size_bytes = ToByte(block.first);
            ret_val.insert(ret_val.end(), block_size_bytes.begin(), block_size_bytes.end());

            auto block_hash_bytes = block.second.Serialize();
            ret_val.insert(ret_val.end(), block_hash_bytes.begin(), block_hash_bytes.end());
        }

        for(auto& asset : asset_map)
        {
            auto uuid_bytes = asset.first.Serialize();
            ret_val.insert(ret_val.end(), uuid_bytes.begin(), uuid_bytes.end());

            auto asset_header_bytes = asset.second.Serialize();
            ret_val.insert(ret_val.end(), asset_header_bytes.begin(), asset_header_bytes.end());
        }

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        if(bytes.size() < sizeof(u32) * 2) // Two counts at the start
        {
            throw std::out_of_range("Incorrect amount of bytes supplied to deserialize icpak_index");
            return;
        }

        size_t used_bytes = 0;

        u32bytes block_count_bytes;
        ReadBytes<u32bytes>(bytes, used_bytes, block_count_bytes);
        u32 block_count = FromByte(block_count_bytes);

        u32bytes asset_count_bytes;
        ReadBytes<u32bytes>(bytes, used_bytes, asset_count_bytes);
        u32 asset_count = FromByte(asset_count_bytes);

        if(block_count > ICPAK_MAX_BLOCK_COUNT)
        {
            throw std::out_of_range("Block count is larger than allowed maximum");
        }

        if(asset_count > ICPAK_MAX_ASSET_COUNT)
        {
            throw std::out_of_range("Asset count is larger than allowed maximum");
        }

        if(bytes.size() != byte_size(block_count, asset_count))
        {
            throw(std::out_of_range("Incorrect amount of bytes supplied to deserialize icpak_index"));
            return;
        }
        
        blocks.clear();
        asset_map.clear();

        blocks.reserve(block_count);

        for(u32 i = 0; i < block_count; ++i)
        {
            u32bytes block_size_bytes;
            ReadBytes<u32bytes>(bytes, used_bytes, block_size_bytes);
            u32 block_size = FromByte(block_size_bytes);

            sha256_hash hash = sha256_hash();
            hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
            used_bytes += sha256_hash::byte_size;

            blocks.push_back(std::make_pair(block_size, hash));
        }

        for(u32 i = 0; i < asset_count; ++i)
        {
            auto uuid = icpak_uuid();
            uuid.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_uuid::byte_size));
            used_bytes += icpak_uuid::byte_size;

            auto header = icpak_asset_header();
            header.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_asset_header::byte_size));
            used_bytes += icpak_asset_header::byte_size;

            asset_map[uuid] = header;
        }

        return;
    }
};
#pragma endregion
#pragma endregion