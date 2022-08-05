#pragma once

#include <models/miner/miner.h>

namespace logpass {

class BlockHeader;
using BlockHeader_ptr = std::shared_ptr<BlockHeader>;
using BlockHeader_cptr = std::shared_ptr<const BlockHeader>;

class BlockHeader {
    static constexpr std::string_view SIGNATURE_PREFIX = "LOGPASS SIGNED BLOCK:\n";

public:
    BlockHeader() = default;
    BlockHeader(uint32_t id);
    // creates new block header
    BlockHeader(uint32_t id, uint32_t depth, const Hash& prevHeaderHash, const Hash& bodyHash,
                const MinerId& minerId, const MinersQueue& nextMiners, const Signature& signature);
    // creates new block header and signs it with given private key
    BlockHeader(uint32_t id, uint32_t depth, const Hash& prevHeaderHash, const Hash& bodyHash,
                const MinersQueue& nextMiners, const PrivateKey& privateKey);
    BlockHeader(const BlockHeader&) = delete;
    BlockHeader& operator = (const BlockHeader&) = delete;

    void serialize(Serializer& s, bool withSignature = true);

    bool validate(const PublicKey& minerKey) const;

    uint8_t getVersion() const
    {
        return m_version;
    }

    uint32_t getId() const
    {
        return m_id;
    }

    uint32_t getDepth() const
    {
        return m_depth;
    }

    uint8_t getSkippedBlocks() const
    {
        ASSERT(!m_nextMiners.empty());
        return (uint8_t)m_nextMiners.size() - 1;
    }

    MinerId getMiner() const
    {
        return m_minerId;
    }

    MinersQueue getNextMiners() const
    {
        return m_nextMiners;
    }

    Hash getPrevHeaderHash() const
    {
        return m_prevHeaderHash;
    }

    Hash getBodyHash() const
    {
        return m_bodyHash;
    }

    Signature getSignature() const
    {
        return m_signature;
    }

    Hash getHash() const
    {
        return m_hash;
    }

    std::string toString() const;

    void toJSON(json& j) const;

private:
    uint8_t m_version = 1;
    uint32_t m_id = 0;
    uint32_t m_depth = 0;
    Hash m_prevHeaderHash;
    Hash m_bodyHash;
    MinerId m_minerId;
    MinersQueue m_nextMiners;
    Signature m_signature; // all header except signature

    // hash is calculated during object creation
    Hash m_hash;
};

}
