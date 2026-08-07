#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <stdexcept>

#include <openssl/sha.h>
#include <openssl/evp.h>

std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> bytes)
{
    std::array<std::uint8_t, 32> hash;
    SHA256(bytes.data(), bytes.size(), hash.data());
    return hash;
}

// Wrapper for openssl SHA256 generation from a stream
class icfw_SHA256
{
private:
    SHA256_CTX hash_ctx;
public:
    icfw_SHA256()
    {
        SHA256_Init(&hash_ctx);
    }

    void UpdateHash(std::span<const std::uint8_t> bytes)
    {
        SHA256_Update(&hash_ctx, bytes.data(), bytes.size());
    }

    std::array<std::uint8_t, 32> FinishHash()
    {
        unsigned char hash[32];
        SHA256_Final(hash, &hash_ctx);
        return std::to_array<std::uint8_t, 32>(hash);
    }
};