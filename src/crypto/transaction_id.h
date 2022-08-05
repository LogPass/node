#pragma once

#include "hash.h"

namespace logpass {

class TransactionId;
using TransactionId_ptr = std::shared_ptr<TransactionId>;
using TransactionId_cptr = std::shared_ptr<const TransactionId>;

class TransactionId : public CryptoArray<7 + Hash::SIZE> {
    friend class Serializer;

public:
    using CryptoArray::CryptoArray;
    TransactionId(uint8_t type, uint32_t blockId, uint16_t size, const Hash& hash)
    {
        *reinterpret_cast<uint32_t*>(data()) = boost::endian::endian_reverse(blockId);
        *reinterpret_cast<uint8_t*>(data() + 4) = type;
        *reinterpret_cast<uint16_t*>(data() + 5) = boost::endian::endian_reverse(size);
        std::copy(hash.cbegin(), hash.cend(), begin() + 7);
    }

    uint32_t getBlockId() const
    {
        return boost::endian::endian_reverse(*reinterpret_cast<const uint32_t*>(data()));
    }

    uint8_t getType() const
    {
        return *reinterpret_cast<const uint8_t*>(data() + 4);
    }

    uint16_t getSize() const
    {
        return boost::endian::endian_reverse(*reinterpret_cast<const uint16_t*>(data() + 5));
    }

    Hash getHash() const
    {
        Hash hash;
        std::copy(cbegin() + 7, cend(), hash.begin());
        return hash;
    }

protected:
    // don't make size public, it's easy to make mistake with getSize which return size of transaction
    using CryptoArray::size;
};

}
