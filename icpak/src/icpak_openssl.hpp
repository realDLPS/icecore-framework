#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <stdexcept>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

typedef std::array<std::uint8_t, 32> sha256_hash;

sha256_hash sha256(std::span<const std::uint8_t> bytes)
{
    sha256_hash hash;
    SHA256(bytes.data(), bytes.size(), hash.data());
    return hash;
}

// Wrapper for openssl SHA256 generation from a stream
class icpak_SHA256
{
private:
    SHA256_CTX hash_ctx;
public:
    icpak_SHA256()
    {
        SHA256_Init(&hash_ctx);
    }

    void UpdateHash(std::span<const std::uint8_t> bytes)
    {
        SHA256_Update(&hash_ctx, bytes.data(), bytes.size());
    }

    sha256_hash FinishHash()
    {
        unsigned char hash[32];
        SHA256_Final(hash, &hash_ctx);
        return std::to_array<std::uint8_t, 32>(hash);
    }
};

typedef std::array<std::uint8_t, 16> icpak_uuid;

namespace icpak
{
    icpak_uuid generate_uuid()
    {
        icpak_uuid uuid;
        RAND_bytes(uuid.data(), uuid.size());
        return uuid;
    }
}