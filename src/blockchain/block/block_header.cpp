#include "pch.h"
#include "block_header.h"

namespace logpass {

BlockHeader::BlockHeader(uint32_t id) : m_id(id)
{
    m_nextMiners.resize(1);
    Serializer s;
    serialize(s);
    ASSERT(m_hash.isValid());
}

BlockHeader::BlockHeader(uint32_t id, uint32_t depth, const Hash& prevHeaderHash, const Hash& bodyHash,
                         const MinerId& minerId, const MinersQueue& nextMiners, const Signature& signature) :
    m_id(id), m_depth(depth), m_prevHeaderHash(prevHeaderHash), m_bodyHash(bodyHash),
    m_minerId(minerId), m_nextMiners(nextMiners), m_signature(signature)
{
    ASSERT(!nextMiners.empty() && nextMiners.size() <= kMinersQueueSize);
    ASSERT(m_id >= m_depth);

    Serializer s;
    serialize(s);
    ASSERT(m_hash.isValid());
}

BlockHeader::BlockHeader(uint32_t id, uint32_t depth, const Hash& prevHeaderHash, const Hash& bodyHash,
                         const MinersQueue& nextMiners, const PrivateKey& privateKey) :
    m_id(id), m_depth(depth), m_prevHeaderHash(prevHeaderHash), m_bodyHash(bodyHash),
    m_minerId(privateKey.publicKey()), m_nextMiners(nextMiners)
{
    ASSERT(!m_nextMiners.empty() && m_nextMiners.size() <= kMinersQueueSize);
    ASSERT(m_id >= m_depth);

    Serializer s, s2;
    serialize(s, false);
    m_signature = privateKey.sign(SIGNATURE_PREFIX, s);
    serialize(s2);
    ASSERT(m_hash.isValid());
}

void BlockHeader::serialize(Serializer& s, bool withSignature)
{
    ASSERT(s.reader() || !m_nextMiners.empty());
    size_t serializerPos = s.pos();

    s(m_version);
    if (m_version != 1) {
        THROW_SERIALIZER_EXCEPTION("Invalid block header version");
    }

    s(m_id);
    s(m_depth);

    if (m_depth > m_id) {
        THROW_SERIALIZER_EXCEPTION("Invalid block depth");
    }

    s(m_prevHeaderHash);
    s(m_bodyHash);
    s(m_minerId);

    if (!m_minerId.isValid()) {
        THROW_SERIALIZER_EXCEPTION("Invalid miner id");
    }

    s.serialize<uint8_t>(m_nextMiners);
    if (m_nextMiners.empty() || m_nextMiners.size() > kMinersQueueSize) {
        THROW_SERIALIZER_EXCEPTION("Invalid number of next miners");
    }

    if (withSignature) {
        s(m_signature);
        if (!m_hash.isValid()) {
            m_hash = Hash(s.begin() + serializerPos, s.pos() - serializerPos);
        }
    }
}

bool BlockHeader::validate(const PublicKey& minerKey) const
{
    Serializer s;
    s(m_version);
    s(m_id);
    s(m_depth);
    s(m_prevHeaderHash);
    s(m_bodyHash);
    s(m_minerId);
    s.serialize<uint8_t>(m_nextMiners);
    return minerKey.verify(SIGNATURE_PREFIX, s, m_signature);
}

std::string BlockHeader::toString() const
{
    return std::to_string(m_id) + "-"s + std::to_string(m_depth) + "-"s + m_hash.toString();
}

void BlockHeader::toJSON(json& j) const
{
    j["version"] = m_version;
    j["id"] = m_id;
    j["depth"] = m_depth;
    j["next_miners"] = m_nextMiners;
    j["prev_header_hash"] = m_prevHeaderHash;
    j["body_hash"] = m_bodyHash;
    j["signature"] = m_signature;
    j["miner_id"] = m_minerId;
    j["hash"] = m_hash;
}

}
