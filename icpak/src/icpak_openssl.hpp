#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <stdexcept>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "icpak_types.hpp"

sha256_hash sha256(std::span<const std::uint8_t> bytes)
{
    sha256_hash hash;
    SHA256(bytes.data(), bytes.size(), hash.hash_bytes.data());
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

    void UpdateHash(std::span<const byte> bytes)
    {
        SHA256_Update(&hash_ctx, bytes.data(), bytes.size());
    }

    sha256_hash FinishHash()
    {
        unsigned char hash[32];
        SHA256_Final(hash, &hash_ctx);
        return sha256_hash(std::to_array<byte, sha256_hash_size>(hash));
    }
};

namespace icpak
{
    icpak_uuid generate_uuid()
    {
        icpak_uuid uuid;
        RAND_bytes(uuid.data(), uuid.size());
        return uuid;
    }
}