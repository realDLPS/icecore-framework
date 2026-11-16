#pragma once

#include <array>
#include <string>
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

//#define BUILD_TEST

#if defined(BUILD_TEST)
#include <random> // used to test uuids
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
// No real logic to this, 16384 just seemed like enough assets for a single pak as a default.
//
// You may specify another count by defining this yourself.
#define ICPAK_MAX_ASSET_COUNT 16384 
#endif

#ifndef ICPAK_MAX_STRING_LENGTH
// Maximum byte length of any string (paths, icpak names) stored in the formats.
// You may specify another length by defining this yourself.
#define ICPAK_MAX_STRING_LENGTH 4096
#endif

#ifndef ICPAK_MAX_SIGNATURE_SIZE
// Maximum byte size of the TOC signature. Ed25519 only needs 64, this leaves room for other algorithms.
// You may specify another length by defining this yourself.
#define ICPAK_MAX_SIGNATURE_SIZE 1024
#endif

#ifndef ICPAK_TOC_MAX_ICPAK_COUNT
// Maximum number of icpaks (and thereby icpak indecies) a single TOC or depot manifest may refer to.
//
// You could specify another length by defining this yourself. Though I don't see the need for it,
// especially since with the default blocks you could reach 16 TiB and with max size blocks
// 64 PiB.
#define ICPAK_TOC_MAX_ICPAK_COUNT 1024
#endif

// The TOC covers every icpak, so these are the per-pak limits times the icpak count. (Used for the TOC and the depot manifest)
#define ICPAK_TOC_MAX_ASSET_COUNT ((u64)ICPAK_MAX_ASSET_COUNT * (u64)ICPAK_TOC_MAX_ICPAK_COUNT)
#define ICPAK_TOC_MAX_BLOCK_COUNT ((u64)ICPAK_MAX_BLOCK_COUNT * (u64)ICPAK_TOC_MAX_ICPAK_COUNT)

#define TOC_VERSION 0
#define ICPAK_VERSION 0
#define ICPAK_INDEX_VERSION 0
#define ICPAK_ASSET_VERSION 0
#define ICPAK_ASSET_HEADER_VERSION 0

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
inline byte_span SafeSubSpan(byte_span bytes, size_t offset, size_t count)
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

// Appends a u32 as big endian
inline void AppendU32(std::vector<byte>& out, u32 value)
{
    auto bytes = ToByte(value);
    out.insert(out.end(), bytes.begin(), bytes.end());
}
// Reads a big endian u32 and advances the offset
inline u32 ReadU32(byte_span bytes, size_t& offset)
{
    u32bytes value_bytes;
    ReadBytes<u32bytes>(bytes, offset, value_bytes);
    return FromByte(value_bytes);
}
// Appends raw bytes
inline void AppendBytes(std::vector<byte>& out, byte_span data)
{
    out.insert(out.end(), data.begin(), data.end());
}
// Strings are stored as a big endian u32 length followed by that many bytes (no null terminator).
inline void AppendString(std::vector<byte>& out, const std::string& str)
{
    if(str.size() > ICPAK_MAX_STRING_LENGTH)
    {
        throw std::length_error("String is longer than ICPAK_MAX_STRING_LENGTH");
    }
    AppendU32(out, (u32)str.size());
    for(char c : str)
    {
        out.push_back((byte)c);
    }
}
inline std::string ReadString(byte_span bytes, size_t& offset)
{
    u32 length = ReadU32(bytes, offset);
    if(length > ICPAK_MAX_STRING_LENGTH)
    {
        throw std::out_of_range("String is longer than ICPAK_MAX_STRING_LENGTH");
    }
    if(length == 0)
    {
        return std::string();
    }
    byte_span str_bytes = SafeSubSpan(bytes, offset, length);
    offset += length;
    return std::string(reinterpret_cast<const char*>(str_bytes.data()), str_bytes.size());
}
// Throws if there is unread data left over, used at the end of Deserialize.
inline void ExpectFullyConsumed(byte_span bytes, size_t used)
{
    if(used != bytes.size())
    {
        throw std::out_of_range("Incorrect amount of bytes supplied, there is data left over after deserializing");
    }
}

#pragma region Magic numbers and file extensions
// Every file type starts with a magic number to identify it (big endian, so they read as ASCII in a hex editor).
constexpr u32 MakeMagic(char a, char b, char c, char d)
{
    return ((u32)(byte)a << 24) | ((u32)(byte)b << 16) | ((u32)(byte)c << 8) | (u32)(byte)d;
}

namespace icpak {
// NOTE: The numbers were "to be defined", these are placeholders and may be changed freely before version 1.

// Runtime
inline constexpr u32 MAGIC_TOC          = MakeMagic('I', 'C', 'T', 'C');
inline constexpr u32 MAGIC_ICPAK_INDEX  = MakeMagic('I', 'C', 'I', 'X');
// Reserved. An .icpak is nothing but blocks (the offset of block N is the sum of the sizes of the blocks before it, see icpak_index::CalculateBlockOffset)
// so that every block stays aligned for steampipe. A prefix would shift all of them, so this is NOT written to the file by default.
inline constexpr u32 MAGIC_ICPAK        = MakeMagic('I', 'C', 'P', 'K');

// Packer
inline constexpr u32 MAGIC_ICMAN        = MakeMagic('I', 'C', 'M', 'N');
inline constexpr u32 MAGIC_ICBLOCK      = MakeMagic('I', 'C', 'B', 'L');
inline constexpr u32 MAGIC_ICAST        = MakeMagic('I', 'C', 'A', 'S');
inline constexpr u32 MAGIC_ICAST_BIN    = MakeMagic('I', 'C', 'A', 'B');

// Throws if the next four bytes are not the expected magic number, otherwise advances the offset past it.
inline void ExpectMagic(byte_span bytes, size_t& offset, u32 magic)
{
    if(ReadU32(bytes, offset) != magic)
    {
        throw std::invalid_argument("Magic number mismatch, this is not the expected file type or the file is corrupted");
    }
}

namespace ext {
// Runtime
inline constexpr const char* toc         = ".toc";
inline constexpr const char* icpak       = ".icpak";
inline constexpr const char* icpak_index = ".icpak-index";
// Packer
inline constexpr const char* icman       = ".icman";
inline constexpr const char* icblock     = ".icblock";
inline constexpr const char* icast       = ".icast";
inline constexpr const char* icast_bin   = ".icast-bin";
}
}
#pragma endregion

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
        hash1.data[i] = i;
        hash2.data[i] = i;
    }

    CHECK(hash1 == hash2);
}
TEST_CASE("Testing hash comparison #2") {
    
    sha256_hash hash1;
    sha256_hash hash2;

    for(u32 i = 0; i < sha256_hash_size; ++i)
    {
        hash1.data[i] = i;
        hash2.data[i] = sha256_hash_size - i;
    }

    CHECK(!(hash1 == hash2));
}
TEST_CASE("Testing hash serialization") {
    
    sha256_hash hash1;

    for(u32 i = 0; i < sha256_hash_size; ++i)
    {
        hash1.data[i] = i;
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

    bool operator<(const icpak_uuid& other) const
    {
        return data < other.data;
    }
};
inline const bool operator==(const icpak_uuid &lhs, const icpak_uuid &rhs)
{
    return std::ranges::equal(lhs.data, rhs.data);
}

#if defined(BUILD_TEST)
// Generates a UUID using std instead of openssl, only for testing
inline icpak_uuid GenerateTempUuid()
{
    std::random_device random_source;
    icpak_uuid uuid = icpak_uuid();
    for(size_t i = 0; i < icpak_uuid_size; i += sizeof(u32))
    {
        u32bytes chunk = ToByte((u32)random_source());
        for(size_t j = 0; j < chunk.size(); ++j)
        {
            uuid.data[i + j] = chunk[j];
        }
    }
    return uuid;
}

TEST_CASE("Checking UUID comparison") {
    icpak_uuid uuid1;
    icpak_uuid uuid2;

    for(u32 i = 0; i < icpak_uuid_size; ++i)
    {
        uuid1.data[i] = i;
        uuid2.data[i] = i;
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
    original.compressed_hash.data = {1};
    original.uncompressed_hash.data = {2};

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

    // What an .icpak-index file contains: the magic number followed by Serialize().
    // The hash stored in the TOC is the hash of these bytes.
    std::vector<byte> SerializeFile() const
    {
        std::vector<byte> ret_val;
        AppendU32(ret_val, icpak::MAGIC_ICPAK_INDEX);

        auto body = Serialize();
        ret_val.insert(ret_val.end(), body.begin(), body.end());

        return ret_val;
    }
    void DeserializeFile(std::span<const byte> bytes)
    {
        size_t used_bytes = 0;
        icpak::ExpectMagic(bytes, used_bytes, icpak::MAGIC_ICPAK_INDEX);
        Deserialize(SafeSubSpan(bytes, used_bytes, bytes.size() - used_bytes));
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





#pragma region Table of contents
namespace icpak {
// Ver2: The TOC can be signed, and since it holds the hashes of every .icpak-index (which in turn hold the hashes of every block)
// a valid signature on the TOC in turn signs everything.
enum signature_type : u32
{
    SIGNATURE_NONE = 0,
    SIGNATURE_ED25519 = 1 // 64 byte signature
};
}

// The .toc file
// The binary holds the hash of the TOC (the entire file, signature included) that it is expecting.
struct ICPAK_TOC : Serializable
{
    /* Version information */
    // Before version 1 these will be set to 0 regardless of changes.
    u32 toc_version = 0;
    u32 icpak_version = 0;
    u32 icpak_index_version = 0;
    u32 asset_format_version = 0;
    u32 asset_header_version = 0;
    
    u32 toc_block_size = 0; // How large the blocks were when this toc was made.

    // Key: Name of the .icpak-index file, Value: Hash of that file (to identify corruption or modification)
    std::map<std::string, sha256_hash> icpak_index_hashes = std::map<std::string, sha256_hash>();
    // Key: Asset UUID, Value: Name of the .icpak-index file the asset manager should look into to find the asset header.
    // The value must always be a key of icpak_index_hashes.
    std::map<icpak_uuid, std::string> asset_locations = std::map<icpak_uuid, std::string>();

    // Signature over the bytes returned by SerializeSignedRegion(). The signing and verifying itself
    // is up to the packer and runtime respectively, this type only stores the result.
    icpak::signature_type signature_type = icpak::SIGNATURE_NONE;
    std::vector<byte> signature = std::vector<byte>();

    bool IsSigned() const
    {
        return signature_type != icpak::SIGNATURE_NONE;
    }

    // Everything the signature covers, which is everything in the file except the signature itself.
    //
    // Layout (all integers big endian):
    //   magic, the 5 versions, toc_block_size, icpak count, asset count,
    //   per icpak: (string name, sha256 index hash),
    //   per asset: (uuid, u32 index into the icpak list above)
    // Names are stored once and assets refer to them by position, so that the TOC stays small.
    std::vector<byte> SerializeSignedRegion() const
    {
        if(icpak_index_hashes.size() > ICPAK_TOC_MAX_ICPAK_COUNT)
        {
            throw std::length_error("Too many icpaks in TOC");
        }
        if(asset_locations.size() > ICPAK_TOC_MAX_ASSET_COUNT)
        {
            throw std::length_error("Too many assets in TOC");
        }

        std::vector<byte> ret_val;

        AppendU32(ret_val, icpak::MAGIC_TOC);
        AppendU32(ret_val, toc_version);
        AppendU32(ret_val, icpak_version);
        AppendU32(ret_val, icpak_index_version);
        AppendU32(ret_val, asset_format_version);
        AppendU32(ret_val, asset_header_version);
        AppendU32(ret_val, toc_block_size);
        AppendU32(ret_val, (u32)icpak_index_hashes.size());
        AppendU32(ret_val, (u32)asset_locations.size());

        std::map<std::string, u32> name_to_position;
        u32 position = 0;
        for(const auto& entry : icpak_index_hashes)
        {
            AppendString(ret_val, entry.first);
            AppendBytes(ret_val, entry.second.data);
            name_to_position[entry.first] = position++;
        }

        for(const auto& entry : asset_locations)
        {
            auto found = name_to_position.find(entry.second);
            if(found == name_to_position.end())
            {
                throw std::invalid_argument("An asset location refers to an icpak that has no index hash in the TOC");
            }
            AppendBytes(ret_val, entry.first.data);
            AppendU32(ret_val, found->second);
        }

        return ret_val;
    }

    std::vector<byte> Serialize() const
    {
        if(!IsSigned() && !signature.empty())
        {
            throw std::invalid_argument("TOC has a signature but no signature type");
        }
        if(signature.size() > ICPAK_MAX_SIGNATURE_SIZE)
        {
            throw std::length_error("Signature is larger than ICPAK_MAX_SIGNATURE_SIZE");
        }

        std::vector<byte> ret_val = SerializeSignedRegion();

        AppendU32(ret_val, (u32)signature_type);
        AppendU32(ret_val, (u32)signature.size());
        AppendBytes(ret_val, signature);

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t used_bytes = 0;

        icpak::ExpectMagic(bytes, used_bytes, icpak::MAGIC_TOC);

        // Read into locals first so this object is left untouched if the data turns out to be invalid.
        u32 new_toc_version = ReadU32(bytes, used_bytes);
        u32 new_icpak_version = ReadU32(bytes, used_bytes);
        u32 new_icpak_index_version = ReadU32(bytes, used_bytes);
        u32 new_asset_format_version = ReadU32(bytes, used_bytes);
        u32 new_asset_header_version = ReadU32(bytes, used_bytes);
        u32 new_toc_block_size = ReadU32(bytes, used_bytes);

        u32 icpak_count = ReadU32(bytes, used_bytes);
        u32 asset_count = ReadU32(bytes, used_bytes);

        if((u64)icpak_count > (u64)ICPAK_TOC_MAX_ICPAK_COUNT)
        {
            throw std::out_of_range("Icpak count is larger than allowed maximum");
        }
        if((u64)asset_count > (u64)ICPAK_TOC_MAX_ASSET_COUNT)
        {
            throw std::out_of_range("Asset count is larger than allowed maximum");
        }

        std::map<std::string, sha256_hash> new_index_hashes;
        std::vector<std::string> names_by_position;
        names_by_position.reserve(icpak_count);

        for(u32 i = 0; i < icpak_count; ++i)
        {
            std::string name = ReadString(bytes, used_bytes);

            sha256_hash hash = sha256_hash();
            hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
            used_bytes += sha256_hash::byte_size;

            if(!new_index_hashes.emplace(name, hash).second)
            {
                throw std::invalid_argument("Duplicate icpak name in TOC");
            }
            names_by_position.push_back(name);
        }

        std::map<icpak_uuid, std::string> new_asset_locations;

        for(u32 i = 0; i < asset_count; ++i)
        {
            icpak_uuid uuid = icpak_uuid();
            uuid.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_uuid::byte_size));
            used_bytes += icpak_uuid::byte_size;

            u32 position = ReadU32(bytes, used_bytes);
            if(position >= names_by_position.size())
            {
                throw std::out_of_range("Asset location refers to an icpak that is not in the TOC");
            }

            if(!new_asset_locations.emplace(uuid, names_by_position[position]).second)
            {
                throw std::invalid_argument("Duplicate asset UUID in TOC");
            }
        }

        u32 raw_signature_type = ReadU32(bytes, used_bytes);
        if(raw_signature_type > (u32)icpak::SIGNATURE_ED25519)
        {
            throw std::invalid_argument("Unknown signature type in TOC");
        }

        u32 signature_size = ReadU32(bytes, used_bytes);
        if(signature_size > ICPAK_MAX_SIGNATURE_SIZE)
        {
            throw std::out_of_range("Signature is larger than allowed maximum");
        }
        if(raw_signature_type == (u32)icpak::SIGNATURE_NONE && signature_size != 0)
        {
            throw std::invalid_argument("TOC has a signature but no signature type");
        }
        byte_span signature_bytes = SafeSubSpan(bytes, used_bytes, signature_size);
        used_bytes += signature_size;

        ExpectFullyConsumed(bytes, used_bytes);

        toc_version = new_toc_version;
        icpak_version = new_icpak_version;
        icpak_index_version = new_icpak_index_version;
        asset_format_version = new_asset_format_version;
        asset_header_version = new_asset_header_version;
        toc_block_size = new_toc_block_size;
        icpak_index_hashes = std::move(new_index_hashes);
        asset_locations = std::move(new_asset_locations);
        signature_type = (icpak::signature_type)raw_signature_type;
        signature.assign(signature_bytes.begin(), signature_bytes.end());
    }
};
#if defined(BUILD_TEST)
TEST_CASE("Testing icpak_index file magic") {
    icpak_index original = icpak_index();
    icpak_asset_header header = icpak_asset_header();
    header.block = 0;
    original.blocks.push_back(std::make_pair((u32)1, sha256_hash()));
    original.asset_map[icpak_uuid()] = header;

    auto bytes = original.SerializeFile();
    icpak_index converted = icpak_index();
    converted.DeserializeFile(bytes);

    CHECK(converted.blocks.size() == 1);
    CHECK(converted.asset_map.size() == 1);
    CHECK(converted.asset_map.begin()->second == header);

    bytes[0] ^= 0xFF; // Break the magic number
    CHECK_THROWS(converted.DeserializeFile(bytes));
}
TEST_CASE("Testing TOC serialization") {
    ICPAK_TOC original = ICPAK_TOC();
    original.toc_block_size = BLOCK_SIZE;

    sha256_hash hash_a = sha256_hash();
    hash_a.data = {1};
    sha256_hash hash_b = sha256_hash();
    hash_b.data = {2};
    original.icpak_index_hashes["a.icpak-index"] = hash_a;
    original.icpak_index_hashes["b.icpak-index"] = hash_b;

    icpak_uuid uuid_1 = icpak_uuid();
    uuid_1.data = {1};
    icpak_uuid uuid_2 = icpak_uuid();
    uuid_2.data = {2};
    original.asset_locations[uuid_1] = "b.icpak-index";
    original.asset_locations[uuid_2] = "a.icpak-index";

    original.signature_type = icpak::SIGNATURE_ED25519;
    original.signature = std::vector<byte>(64, 7);

    auto bytes = original.Serialize();
    ICPAK_TOC converted = ICPAK_TOC();
    converted.Deserialize(bytes);

    CHECK(converted.toc_block_size == original.toc_block_size);
    CHECK(converted.icpak_index_hashes.size() == 2);
    CHECK(converted.icpak_index_hashes["a.icpak-index"] == hash_a);
    CHECK(converted.icpak_index_hashes["b.icpak-index"] == hash_b);
    CHECK(converted.asset_locations[uuid_1] == "b.icpak-index");
    CHECK(converted.asset_locations[uuid_2] == "a.icpak-index");
    CHECK(converted.signature_type == icpak::SIGNATURE_ED25519);
    CHECK(converted.signature == original.signature);

    // The signed region must be a prefix of the file and not depend on the signature
    auto signed_region = original.SerializeSignedRegion();
    CHECK(signed_region.size() < bytes.size());
    CHECK(std::equal(signed_region.begin(), signed_region.end(), bytes.begin()));
}
TEST_CASE("Testing TOC rejects asset in unknown icpak") {
    ICPAK_TOC toc = ICPAK_TOC();
    toc.asset_locations[icpak_uuid()] = "missing.icpak-index";

    CHECK_THROWS(toc.Serialize());
}
TEST_CASE("Testing TOC rejects truncated and trailing data") {
    ICPAK_TOC original = ICPAK_TOC();
    original.icpak_index_hashes["a.icpak-index"] = sha256_hash();
    original.asset_locations[icpak_uuid()] = "a.icpak-index";

    auto bytes = original.Serialize();

    ICPAK_TOC converted = ICPAK_TOC();

    auto truncated = bytes;
    truncated.pop_back();
    CHECK_THROWS(converted.Deserialize(truncated));

    auto trailing = bytes;
    trailing.push_back(0);
    CHECK_THROWS(converted.Deserialize(trailing));
}
#endif
#if defined(BUILD_TEST)
TEST_CASE("Testing that none of the toc versions are 0") {
    CHECK(TOC_VERSION != 0);
    CHECK(ICPAK_VERSION != 0);
    CHECK(ICPAK_INDEX_VERSION != 0);
    CHECK(ICPAK_ASSET_VERSION != 0);
    CHECK(ICPAK_ASSET_HEADER_VERSION != 0);
    // This test is expected to fail until version 1.
}
#endif

#pragma endregion

#pragma endregion




// These types are only used by the packer (ICPAKER) at development/build time. They never ship to the player.
#pragma region Packer

namespace icpak {
// The packer keeps two depots (versions of the packed content). The packer never pulls blocks between depots,
// so unreleased content can't leak through dead assets that ended up in a block during development.
enum depot_type : byte
{
    DEPOT_DEBUG = 0,  // Assets you might not want in release builds, upcoming content etc.
    DEPOT_RELEASE = 1 // Content that will be released to users in this build or has been available to them before.
};

enum build_mode : byte
{
    BUILD_MODE_STANDARD = 0, // Dead assets are kept in their blocks until purged (see icpak_packer_config)
    BUILD_MODE_WEB = 1       // No dead bytes are stored at all, any block containing a dead asset is purged to make the build as small as possible.
};

// Manual purging via an argument passed to the packer
enum manual_purge_mode : byte
{
    PURGE_NONE = 0,                    // Only purge according to the automatic rules
    PURGE_BLOCKS_WITH_DEAD_ASSETS = 1, // Purge every block that contains at least one dead asset
    PURGE_ALL_BLOCKS = 2               // Purge every block
};
}

#pragma region Packer config
// Settings for a packing run. This is a project config and not a file format.
struct icpak_packer_config
{
    u32 block_size = BLOCK_SIZE;
    icpak::build_mode mode = icpak::BUILD_MODE_STANDARD;
    icpak::manual_purge_mode manual_purge = icpak::PURGE_NONE;

    // A block is purged automatically when less than this percentage (0-100) of its used space is still live assets.
    // Example with 50: A: 100kb, B: 100kb, C: 300kb -> A and B dead keeps the block (60% live), C dead purges it (40% live).
    // The default is only a placeholder taken from the examples in the design notes.
    u32 min_live_percent = 50;

    // If the dead bytes of the entire depot reach this percentage (0-100) of its used space, every block containing dead assets is purged.
    // The default is only a placeholder taken from the examples in the design notes.
    u32 waste_threshold_percent = 20;
};
#pragma endregion

#pragma region icast
// The .icast file, the development time description of an asset.
// Holds more information, less efficiently than the packed formats. (Which depot the asset belongs to is decided by which folder the file is in.)
struct icpak_icast : Serializable
{
    icpak_uuid uuid = icpak_uuid();
    std::string content_path = std::string(); // Path to the actual asset in the content folder
    std::string icpak_name = std::string();   // Which icpak the asset will go into, the user decides this
    
    // What the packer needs to build the asset header
    icpak_asset_type asset_type = 0;
    icpak::compression_type compression_type = icpak::UNCOMPRESSED;
    byte asset_flags = 0;

    std::vector<byte> Serialize() const
    {
        std::vector<byte> ret_val;

        AppendU32(ret_val, icpak::MAGIC_ICAST);
        AppendBytes(ret_val, uuid.data);
        AppendString(ret_val, content_path);
        AppendString(ret_val, icpak_name);
        ret_val.push_back(asset_type);
        ret_val.push_back(compression_type);
        ret_val.push_back(asset_flags);

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t used_bytes = 0;

        icpak::ExpectMagic(bytes, used_bytes, icpak::MAGIC_ICAST);

        icpak_uuid new_uuid = icpak_uuid();
        new_uuid.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_uuid::byte_size));
        used_bytes += icpak_uuid::byte_size;

        std::string new_content_path = ReadString(bytes, used_bytes);
        std::string new_icpak_name = ReadString(bytes, used_bytes);

        byte new_asset_type = 0;
        ReadBytes<byte>(bytes, used_bytes, new_asset_type);
        byte new_compression_type = 0;
        ReadBytes<byte>(bytes, used_bytes, new_compression_type);
        byte new_asset_flags = 0;
        ReadBytes<byte>(bytes, used_bytes, new_asset_flags);

        ExpectFullyConsumed(bytes, used_bytes);

        uuid = new_uuid;
        content_path = std::move(new_content_path);
        icpak_name = std::move(new_icpak_name);
        asset_type = new_asset_type;
        compression_type = (icpak::compression_type)new_compression_type;
        asset_flags = new_asset_flags;
    }
};
#if defined(BUILD_TEST)
TEST_CASE("Testing icast serialization") {
    icpak_icast original = icpak_icast();
    original.uuid = GenerateUuid();
    original.content_path = "content/player.png";
    original.icpak_name = "base";
    original.asset_type = 3;
    original.asset_flags = 1;

    auto bytes = original.Serialize();
    icpak_icast converted = icpak_icast();
    converted.Deserialize(bytes);

    CHECK(converted.uuid == original.uuid);
    CHECK(converted.content_path == original.content_path);
    CHECK(converted.icpak_name == original.icpak_name);
    CHECK(converted.asset_type == original.asset_type);
    CHECK(converted.asset_flags == original.asset_flags);
}
#endif
#pragma endregion

#pragma region icast-bin
// The .icast-bin file, holds the binary data of an asset. (Compressed source asset)
// The packer copies this into a block and uses the metadata to fill in the asset header.
struct icpak_icast_bin : Serializable
{
    icpak::compression_type compression_type = icpak::UNCOMPRESSED;
    u32 size = 0; // Byte size of the asset after decompression
    sha256_hash compressed_hash = sha256_hash();
    sha256_hash uncompressed_hash = sha256_hash();
    std::vector<byte> data = std::vector<byte>(); // The compressed bytes, the compressed size is data.size()

    std::vector<byte> Serialize() const
    {
        if(data.size() > std::numeric_limits<u32>::max())
        {
            throw std::length_error("Asset is too large, sizes are 32 bit");
        }

        std::vector<byte> ret_val;
        ret_val.reserve(data.size() + 128);

        AppendU32(ret_val, icpak::MAGIC_ICAST_BIN);
        ret_val.push_back(compression_type);
        AppendU32(ret_val, size);
        AppendBytes(ret_val, compressed_hash.data);
        AppendBytes(ret_val, uncompressed_hash.data);
        AppendU32(ret_val, (u32)data.size());
        AppendBytes(ret_val, data);

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t used_bytes = 0;

        icpak::ExpectMagic(bytes, used_bytes, icpak::MAGIC_ICAST_BIN);

        byte new_compression_type = 0;
        ReadBytes<byte>(bytes, used_bytes, new_compression_type);

        u32 new_size = ReadU32(bytes, used_bytes);

        sha256_hash new_compressed_hash = sha256_hash();
        new_compressed_hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
        used_bytes += sha256_hash::byte_size;

        sha256_hash new_uncompressed_hash = sha256_hash();
        new_uncompressed_hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
        used_bytes += sha256_hash::byte_size;

        u32 data_size = ReadU32(bytes, used_bytes);
        byte_span data_bytes = SafeSubSpan(bytes, used_bytes, data_size);
        used_bytes += data_size;

        ExpectFullyConsumed(bytes, used_bytes);

        compression_type = (icpak::compression_type)new_compression_type;
        size = new_size;
        compressed_hash = new_compressed_hash;
        uncompressed_hash = new_uncompressed_hash;
        data.assign(data_bytes.begin(), data_bytes.end());
    }
};
#if defined(BUILD_TEST)
TEST_CASE("Testing icast-bin serialization") {
    icpak_icast_bin original = icpak_icast_bin();
    original.size = 5;
    original.compressed_hash.data = {1};
    original.uncompressed_hash.data = {2};
    original.data = {1, 2, 3};

    auto bytes = original.Serialize();
    icpak_icast_bin converted = icpak_icast_bin();
    converted.Deserialize(bytes);

    CHECK(converted.size == original.size);
    CHECK(converted.compressed_hash == original.compressed_hash);
    CHECK(converted.uncompressed_hash == original.uncompressed_hash);
    CHECK(converted.data == original.data);
}
#endif
#pragma endregion

#pragma region icblock
// One asset stored in a block. Blocks are immutable, so this list never changes after the block is created.
// (This replaces the old "block-buddy" bookkeeping, the block itself knows which assets it holds.)
struct icpak_block_asset : virtual Serializable_SK
{
    icpak_uuid uuid = icpak_uuid();
    u32 offset = 0;                    // Byte offset inside the block, mirrors icpak_asset_header::offset
    u32 size = 0;                      // Bytes the asset takes up in the block (its compressed size)
    sha256_hash hash = sha256_hash();  // The asset hash as stored in the depot manifest when the asset was packed into this block

    static constexpr size_t byte_size = icpak_uuid::byte_size + sizeof(u32) + sizeof(u32) + sha256_hash::byte_size;

    std::vector<byte> Serialize() const
    {
        std::vector<byte> ret_val;
        ret_val.reserve(byte_size);

        AppendBytes(ret_val, uuid.data);
        AppendU32(ret_val, offset);
        AppendU32(ret_val, size);
        AppendBytes(ret_val, hash.data);

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        if(bytes.size() != byte_size)
        {
            throw(std::out_of_range("Incorrect number of bytes supplied to deserialize block asset."));
            return;
        }
        size_t used_bytes = 0;

        uuid.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_uuid::byte_size));
        used_bytes += icpak_uuid::byte_size;

        offset = ReadU32(bytes, used_bytes);
        size = ReadU32(bytes, used_bytes);

        hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
        used_bytes += sha256_hash::byte_size;
    }
};

// The .icblock file, an already built block that the packer copies into an .icpak when packing.
// Blocks are immutable: they never accept new assets after creation and are never edited, only deleted by a purge.
struct icpak_icblock : Serializable
{
    u32 size_multiple = 1; // The block is size_multiple * block size bytes, mirrors icpak_index::blocks
    sha256_hash hash = sha256_hash(); // Hash of data, mirrors icpak_index::blocks
    std::vector<icpak_block_asset> assets = std::vector<icpak_block_asset>(); // Every asset in the block, dead or alive (see icpak_manifest::IsLive)
    std::vector<byte> data = std::vector<byte>(); // The block as it will appear in the .icpak, dead assets included

    // Checks that data is exactly size_multiple blocks large.
    bool HasValidSize(u32 block_size = BLOCK_SIZE) const
    {
        size_t expected = 0;
        if(size_multiple == 0 || !ValidatedMul((size_t)size_multiple, (size_t)block_size, expected))
        {
            return false;
        }
        return data.size() == expected;
    }

    // Used space is everything the assets take up, dead or alive.
    u64 UsedBytes() const
    {
        u64 total = 0;
        for(const auto& asset : assets) { total += asset.size; }
        return total;
    }

    std::vector<byte> Serialize() const
    {
        if(data.size() > std::numeric_limits<u32>::max())
        {
            throw std::length_error("Block is too large, sizes are 32 bit");
        }
        if(assets.size() > ICPAK_MAX_ASSET_COUNT)
        {
            throw std::length_error("Too many assets in block");
        }

        std::vector<byte> ret_val;
        ret_val.reserve(data.size() + 64 + assets.size() * icpak_block_asset::byte_size);

        AppendU32(ret_val, icpak::MAGIC_ICBLOCK);
        AppendU32(ret_val, size_multiple);
        AppendBytes(ret_val, hash.data);
        AppendU32(ret_val, (u32)assets.size());
        for(const auto& asset : assets)
        {
            auto asset_bytes = asset.Serialize();
            ret_val.insert(ret_val.end(), asset_bytes.begin(), asset_bytes.end());
        }
        AppendU32(ret_val, (u32)data.size());
        AppendBytes(ret_val, data);

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t used_bytes = 0;

        icpak::ExpectMagic(bytes, used_bytes, icpak::MAGIC_ICBLOCK);

        u32 new_size_multiple = ReadU32(bytes, used_bytes);

        sha256_hash new_hash = sha256_hash();
        new_hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
        used_bytes += sha256_hash::byte_size;

        u32 asset_count = ReadU32(bytes, used_bytes);
        if((u64)asset_count > (u64)ICPAK_MAX_ASSET_COUNT)
        {
            throw std::out_of_range("Asset count is larger than allowed maximum");
        }
        std::vector<icpak_block_asset> new_assets;
        new_assets.reserve(asset_count);
        for(u32 i = 0; i < asset_count; ++i)
        {
            icpak_block_asset asset = icpak_block_asset();
            asset.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_block_asset::byte_size));
            used_bytes += icpak_block_asset::byte_size;
            new_assets.push_back(asset);
        }

        u32 data_size = ReadU32(bytes, used_bytes);
        byte_span data_bytes = SafeSubSpan(bytes, used_bytes, data_size);
        used_bytes += data_size;

        ExpectFullyConsumed(bytes, used_bytes);

        size_multiple = new_size_multiple;
        hash = new_hash;
        assets = std::move(new_assets);
        data.assign(data_bytes.begin(), data_bytes.end());
    }
};
#if defined(BUILD_TEST)
TEST_CASE("Testing icblock serialization") {
    icpak_icblock original = icpak_icblock();
    original.size_multiple = 2;
    original.hash.data = {9};
    original.data = std::vector<byte>(2 * 16, 5);
    icpak_block_asset asset = icpak_block_asset();
    asset.uuid.data = {1};
    asset.offset = 4;
    asset.size = 8;
    asset.hash.data = {7};
    original.assets.push_back(asset);

    auto bytes = original.Serialize();
    icpak_icblock converted = icpak_icblock();
    converted.Deserialize(bytes);

    CHECK(converted.size_multiple == 2);
    CHECK(converted.hash == original.hash);
    CHECK(converted.data == original.data);
    CHECK(converted.assets.size() == 1);
    CHECK(converted.assets[0].uuid == asset.uuid);
    CHECK(converted.assets[0].offset == 4);
    CHECK(converted.assets[0].size == 8);
    CHECK(converted.assets[0].hash == asset.hash);
    CHECK(converted.UsedBytes() == 8);
    CHECK(converted.HasValidSize(16));
    CHECK(!converted.HasValidSize(32));
}
#endif
#pragma endregion

#pragma region Depot manifest
// The .icman file, holds the information the packer needs to run. There is one per depot, and it is never included in builds.
struct icpak_manifest : Serializable
{
    icpak::depot_type depot = icpak::DEPOT_RELEASE;
    // Asset UUID -> hash of the asset, used to detect when an asset has changed
    std::map<icpak_uuid, sha256_hash> asset_hashes = std::map<icpak_uuid, sha256_hash>();

    // True if the asset is new to the depot or its hash differs from the one the depot was last packed with.
    bool AssetChanged(const icpak_uuid& uuid, const sha256_hash& current_hash) const
    {
        auto found = asset_hashes.find(uuid);
        if(found == asset_hashes.end())
        {
            return true;
        }
        return !(found->second == current_hash);
    }

    // An asset copy inside a block is live if the depot still has that asset with the same hash.
    // Otherwise it was deleted or changed (and then packed somewhere else), which makes these bytes dead.
    bool IsLive(const icpak_block_asset& asset) const
    {
        auto found = asset_hashes.find(asset.uuid);
        if(found == asset_hashes.end())
        {
            return false;
        }
        return found->second == asset.hash;
    }

    u64 DeadBytes(const icpak_icblock& block) const
    {
        u64 total = 0;
        for(const auto& asset : block.assets) { if(!IsLive(asset)) { total += asset.size; } }
        return total;
    }
    u64 LiveBytes(const icpak_icblock& block) const
    {
        return block.UsedBytes() - DeadBytes(block);
    }
    bool HasDeadAssets(const icpak_icblock& block) const
    {
        for(const auto& asset : block.assets) { if(!IsLive(asset)) { return true; } }
        return false;
    }

    // Automatic purge rule: The block is purged if less than min_live_percent of the used space is still live assets.
    // In the web build mode no dead bytes may be stored at all, so any dead asset purges the block.
    bool ShouldPurge(const icpak_icblock& block, const icpak_packer_config& config) const
    {
        if(config.mode == icpak::BUILD_MODE_WEB)
        {
            return HasDeadAssets(block);
        }
        if(block.assets.empty())
        {
            return false;
        }
        return LiveBytes(block) * 100 < (u64)config.min_live_percent * block.UsedBytes();
    }

    // True if the dead bytes of all the blocks make up at least waste_threshold_percent of the used space.
    bool WasteThresholdReached(const std::vector<icpak_icblock>& blocks, const icpak_packer_config& config) const
    {
        u64 dead = 0;
        u64 used = 0;
        for(const auto& block : blocks)
        {
            dead += DeadBytes(block);
            used += block.UsedBytes();
        }
        if(dead == 0)
        {
            return false;
        }
        return dead * 100 >= (u64)config.waste_threshold_percent * used;
    }

    // Which blocks (as indices into blocks) should be purged. Purging means that all live assets are moved
    // into new blocks and the block is deleted.
    std::vector<size_t> BlocksToPurge(const std::vector<icpak_icblock>& blocks, const icpak_packer_config& config) const
    {
        std::vector<size_t> result;

        // The waste space threshold causes an entire purge of all blocks containing dead assets
        bool purge_all_dead = config.manual_purge == icpak::PURGE_BLOCKS_WITH_DEAD_ASSETS || WasteThresholdReached(blocks, config);

        for(size_t i = 0; i < blocks.size(); ++i)
        {
            bool purge = false;
            if(config.manual_purge == icpak::PURGE_ALL_BLOCKS)
            {
                purge = true;
            }
            else if(purge_all_dead && HasDeadAssets(blocks[i]))
            {
                purge = true;
            }
            else
            {
                purge = ShouldPurge(blocks[i], config);
            }

            if(purge)
            {
                result.push_back(i);
            }
        }

        return result;
    }

    std::vector<byte> Serialize() const
    {
        if(asset_hashes.size() > ICPAK_TOC_MAX_ASSET_COUNT)
        {
            throw std::length_error("Too many assets in manifest");
        }

        std::vector<byte> ret_val;

        AppendU32(ret_val, icpak::MAGIC_ICMAN);
        ret_val.push_back(depot);
        AppendU32(ret_val, (u32)asset_hashes.size());

        for(const auto& entry : asset_hashes)
        {
            AppendBytes(ret_val, entry.first.data);
            AppendBytes(ret_val, entry.second.data);
        }

        return ret_val;
    }
    void Deserialize(std::span<const byte> bytes)
    {
        size_t used_bytes = 0;

        icpak::ExpectMagic(bytes, used_bytes, icpak::MAGIC_ICMAN);

        byte depot_byte = 0;
        ReadBytes<byte>(bytes, used_bytes, depot_byte);
        if(depot_byte > (byte)icpak::DEPOT_RELEASE)
        {
            throw std::invalid_argument("Unknown depot type in manifest");
        }

        u32 asset_count = ReadU32(bytes, used_bytes);
        if((u64)asset_count > (u64)ICPAK_TOC_MAX_ASSET_COUNT)
        {
            throw std::out_of_range("Asset count is larger than allowed maximum");
        }

        std::map<icpak_uuid, sha256_hash> new_asset_hashes;
        for(u32 i = 0; i < asset_count; ++i)
        {
            icpak_uuid uuid = icpak_uuid();
            uuid.Deserialize(SafeSubSpan(bytes, used_bytes, icpak_uuid::byte_size));
            used_bytes += icpak_uuid::byte_size;

            sha256_hash hash = sha256_hash();
            hash.Deserialize(SafeSubSpan(bytes, used_bytes, sha256_hash::byte_size));
            used_bytes += sha256_hash::byte_size;

            if(!new_asset_hashes.emplace(uuid, hash).second)
            {
                throw std::invalid_argument("Duplicate asset UUID in manifest");
            }
        }

        ExpectFullyConsumed(bytes, used_bytes);

        depot = (icpak::depot_type)depot_byte;
        asset_hashes = std::move(new_asset_hashes);
    }
};
#if defined(BUILD_TEST)
// Builds the A: 100kb, B: 100kb, C: 300kb block used as the example in the design notes.
// A dead asset is one that is no longer in the manifest.
static icpak_icblock MakeExampleBlock(icpak_manifest& manifest, bool a_dead, bool b_dead, bool c_dead, byte base = 0)
{
    icpak_icblock block = icpak_icblock();
    const u32 sizes[3] = {100, 100, 300};
    const bool dead[3] = {a_dead, b_dead, c_dead};
    u32 offset = 0;
    for(u32 i = 0; i < 3; ++i)
    {
        icpak_block_asset asset = icpak_block_asset();
        asset.uuid.data = {(byte)(base + i + 1)};
        asset.hash.data = {(byte)(base + i + 1)};
        asset.offset = offset;
        asset.size = sizes[i] * 1024;
        offset += asset.size;
        block.assets.push_back(asset);

        if(!dead[i])
        {
            manifest.asset_hashes[asset.uuid] = asset.hash;
        }
    }
    return block;
}
TEST_CASE("Testing purge parameter 50%") {
    icpak_packer_config config = icpak_packer_config();
    config.min_live_percent = 50;

    icpak_manifest m1 = icpak_manifest();
    CHECK(!m1.ShouldPurge(MakeExampleBlock(m1, true, true, false), config));
    icpak_manifest m2 = icpak_manifest();
    CHECK(m2.ShouldPurge(MakeExampleBlock(m2, false, false, true), config));
}
TEST_CASE("Testing purge parameter 90%") {
    icpak_packer_config config = icpak_packer_config();
    config.min_live_percent = 90;

    icpak_manifest m = icpak_manifest();
    CHECK(m.ShouldPurge(MakeExampleBlock(m, true, false, false), config));
}
TEST_CASE("Testing purge parameter 20%") {
    icpak_packer_config config = icpak_packer_config();
    config.min_live_percent = 20;

    icpak_manifest m1 = icpak_manifest();
    CHECK(!m1.ShouldPurge(MakeExampleBlock(m1, true, true, false), config));
    icpak_manifest m2 = icpak_manifest();
    CHECK(!m2.ShouldPurge(MakeExampleBlock(m2, false, false, true), config));
    icpak_manifest m3 = icpak_manifest();
    CHECK(!m3.ShouldPurge(MakeExampleBlock(m3, false, true, true), config));
    icpak_manifest m4 = icpak_manifest();
    CHECK(m4.ShouldPurge(MakeExampleBlock(m4, true, true, true), config));
}
TEST_CASE("Testing a changed asset hash makes the old copy dead") {
    icpak_manifest manifest = icpak_manifest();
    icpak_icblock block = MakeExampleBlock(manifest, false, false, false);
    CHECK(manifest.DeadBytes(block) == 0);

    manifest.asset_hashes[block.assets[0].uuid].data = {99}; // Asset A changed
    CHECK(manifest.DeadBytes(block) == 100 * 1024);
    CHECK(manifest.LiveBytes(block) == 400 * 1024);
}
TEST_CASE("Testing web build mode purges any dead asset") {
    icpak_packer_config config = icpak_packer_config();
    config.mode = icpak::BUILD_MODE_WEB;

    icpak_manifest m1 = icpak_manifest();
    CHECK(m1.ShouldPurge(MakeExampleBlock(m1, true, false, false), config));
    icpak_manifest m2 = icpak_manifest();
    CHECK(!m2.ShouldPurge(MakeExampleBlock(m2, false, false, false), config));
}
TEST_CASE("Testing manifest serialization") {
    icpak_manifest original = icpak_manifest();
    original.depot = icpak::DEPOT_DEBUG;
    icpak_uuid uuid = GenerateUuid();
    sha256_hash hash = sha256_hash();
    hash.data = {3};
    original.asset_hashes[uuid] = hash;

    auto bytes = original.Serialize();
    icpak_manifest converted = icpak_manifest();
    converted.Deserialize(bytes);

    CHECK(converted.depot == icpak::DEPOT_DEBUG);
    CHECK(converted.asset_hashes.size() == 1);
    CHECK(!converted.AssetChanged(uuid, hash));

    sha256_hash other = sha256_hash();
    other.data = {4};
    CHECK(converted.AssetChanged(uuid, other));
    CHECK(converted.AssetChanged(GenerateUuid(), hash));
}
TEST_CASE("Testing waste threshold purges every block with dead assets") {
    icpak_manifest manifest = icpak_manifest();
    std::vector<icpak_icblock> blocks;
    blocks.push_back(MakeExampleBlock(manifest, true, false, false, 0));  // 100 dead of 500
    blocks.push_back(MakeExampleBlock(manifest, false, false, false, 10));
    blocks.push_back(MakeExampleBlock(manifest, true, false, false, 20)); // 100 dead of 500

    icpak_packer_config config = icpak_packer_config();
    config.min_live_percent = 10; // No block is purged by its own live percentage
    config.waste_threshold_percent = 20; // 200 dead of 1500 is 13%
    CHECK(manifest.BlocksToPurge(blocks, config).empty());

    config.waste_threshold_percent = 10;
    auto purged = manifest.BlocksToPurge(blocks, config);
    CHECK(purged.size() == 2);
    CHECK(purged[0] == 0);
    CHECK(purged[1] == 2);

    config.waste_threshold_percent = 100;
    config.manual_purge = icpak::PURGE_ALL_BLOCKS;
    CHECK(manifest.BlocksToPurge(blocks, config).size() == 3);
}
#endif
#pragma endregion

#pragma endregion