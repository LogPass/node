#pragma once

namespace logpass {

class PostTransactionResult {
public:
    enum class Status : uint8_t {
        SUCCESS,
        DUPLICATED,
        DUPLICATED_BODY,
        REACHED_PENDING_LIMIT,
        SIGNATURE_ERROR,
        SERIALIZER_ERROR,
        OUTDATED,
        VALIDATION_ERROR,
        TIMEOUT,
        DESYNCHRONIZED
    };

    PostTransactionResult(Status status, const std::string& details = "") :
        PostTransactionResult(TransactionId(), status, details)
    {}

    PostTransactionResult(const TransactionId& transactionId, Status status, const std::string& details = "") :
        m_transactionId(transactionId), m_status(status), m_details(details)
    {}

    TransactionId getTransactionId() const
    {
        return m_transactionId;
    }

    std::string getStatus() const
    {
        switch (m_status) {
        case Status::SUCCESS:
            return "SUCCESS";
        case Status::DUPLICATED:
            return "DUPLICATED";
        case Status::DUPLICATED_BODY:
            return "DUPLICATED_BODY";
        case Status::REACHED_PENDING_LIMIT:
            return "REACHED_PENDING_LIMIT";
        case Status::SIGNATURE_ERROR:
            return "SIGNATURE_ERROR";
        case Status::SERIALIZER_ERROR:
            return "SERIALIZER_ERROR";
        case Status::OUTDATED:
            return "OUTDATED";
        case Status::VALIDATION_ERROR:
            return "VALIDATION_ERROR";
        case Status::TIMEOUT:
            return "TIMEOUT";
        case Status::DESYNCHRONIZED:
            return "DESYNCHRONIZED";
        }
        return "unknown";
    }

    std::string getDetails() const
    {
        return m_details;
    }

    bool hasDetails() const
    {
        return !m_details.empty();
    }

    bool operator!() const
    {
        return m_status != Status::SUCCESS;
    }

    friend std::ostream& operator << (std::ostream& out, const PostTransactionResult& result)
    {
        out << result.getStatus();
        if (result.hasDetails()) {
            out << " - " << result.getDetails();
        }
        return out;
    }

    void toJSON(json& j) const
    {
        j["status"] = getStatus();
        j["details"] = getDetails();
        if (m_transactionId.isValid()) {
            j["transaction_id"] = m_transactionId;
        } else {
            j["transaction_id"] = nullptr;
        }
    }

private:
    TransactionId m_transactionId;
    Status m_status;
    std::string m_details;
};

}
