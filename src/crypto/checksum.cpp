#include "pch.h"
#include "hash.h"

namespace logpass {

Hash::Hash(const Serializer& s)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, s.begin(), s.size());
    SHA256_Final(data(), &sha256);
}

Hash::Hash(const uint8_t* begin, const size_t size)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, begin, size);
    SHA256_Final(data(), &sha256);
}

Hash Hash::generate(const std::string_view& s)
{
    return Hash((const uint8_t*)s.data(), s.size());
}

Hash Hash::generateRandom()
{
    Hash s;
    int ret = RAND_bytes(s.data(), Hash::SIZE);
    ASSERT(ret == 1);
    return s;
}

}
