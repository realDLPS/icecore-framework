#include <array>
#include <vector>
#include <algorithm>
#include <ranges>
#include <iterator>
#include <stdexcept>
#include <cstdint>

#define TEST_START if defined(BUILD_TEST)

#if defined(BUILD_TEST)
#include "../../doctest/doctest/doctest.h"
#endif

typedef std::uint8_t byte;
typedef std::uint32_t u32;

// Data-type that can be serialized and deserialized to/from bytes.
struct Serializable
{
    virtual std::vector<byte> Serialize();
    virtual void Deserialize(std::vector<byte> bytes);
};

#pragma region SHA256 hash
// Size of SHA256 hash in bytes
static constexpr u32 sha256_hash_size = 32;
struct sha256_hash : virtual Serializable
{
    std::array<byte, sha256_hash_size> hash_bytes;

    sha256_hash(){};
    sha256_hash(std::array<byte, sha256_hash_size> hash)
    {
        hash_bytes = hash;
    }

    std::vector<byte> Serialize()
    {
        return std::vector<byte>(hash_bytes.begin(), hash_bytes.end());
    }
    void Deserialize(std::vector<byte> bytes)
    {
        if(bytes.size() != sha256_hash_size)
        {
            throw(std::length_error("Incorrect amount of bytes supplied to dezerialize a sha256:hash"));
        }

        std::copy_n(bytes.begin(), sha256_hash_size, hash_bytes.begin());
    }
};
const bool operator==(const sha256_hash &lhs, const sha256_hash &rhs)
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
#endif

#pragma endregion


// Size of a ICPAK UUID in bytes
static constexpr u32 icpak_uuid_size = 16;
typedef std::array<byte, icpak_uuid_size> icpak_uuid;



#pragma region Runtime // These types are required when loaded by the game