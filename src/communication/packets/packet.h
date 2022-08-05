#pragma once

#include "packet_execution_params.h"

namespace logpass {

class Packet;
using Packet_ptr = std::shared_ptr<Packet>;
using Packet_cptr = std::shared_ptr<const Packet>;

class PacketExecutionException : public Exception {
    using Exception::Exception;
};

#define THROW_PACKET_EXCEPTION(message) THROW_EXCEPTION(PacketExecutionException(message))

class Packet : public std::enable_shared_from_this<Packet> {
protected:
    explicit Packet(uint8_t type);

public:
    virtual ~Packet() = default;

    Packet(const Packet& u) = delete;
    Packet& operator= (const Packet& u) = delete;

    static Packet_ptr load(Serializer& s, uint32_t id = 0);

    void serializeRequest(Serializer& s);
    void serializeResponse(Serializer& s);

    void execute(const PacketExecutionParams& params);

    uint8_t getType() const
    {
        return m_type;
    }

    uint32_t getId() const
    {
        return m_id;
    }

    // true for two way packet
    virtual bool requiresResponse() const
    {
        return false;
    }

    // true after serializeResponse is called
    bool hasResponse() const
    {
        return m_hasResponse;
    }

protected:
    virtual void serializeRequestBody(Serializer& s) = 0;
    virtual void serializeResponseBody(Serializer& s)
    {
        ASSERT(!requiresResponse());
    };
    virtual bool validateRequest() = 0;
    virtual bool validateResponse()
    {
        return false;
    };

    virtual void executeRequest(const PacketExecutionParams& params) = 0;
    virtual void executeResponse(const PacketExecutionParams& params)
    {
        ASSERT(!requiresResponse());
    }

    uint8_t m_type = 0;
    uint32_t m_id = 0;
    bool m_hasResponse = false;
    bool m_executedRequest = false;
    bool m_executedResponse = false;
};

// helper class for packet with response
class PacketWithResponse : public Packet {
protected:
    explicit PacketWithResponse(uint8_t type) : Packet(type) {}

public:
    virtual ~PacketWithResponse() = default;

    bool requiresResponse() const override
    {
        return true;
    }

protected:
    virtual void serializeResponseBody(Serializer& s) override = 0;
    virtual bool validateResponse() override = 0;
    virtual void executeResponse(const PacketExecutionParams& params) override = 0;
};

}
