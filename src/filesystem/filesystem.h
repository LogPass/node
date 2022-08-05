#pragma once

namespace logpass {

using SerializerCallback = SafeCallback<Serializer>;

class Filesystem : public EventLoopThread {
protected:
    friend class SharedThread<Filesystem>;

    Filesystem(const fs::path& root, bool m_temporary = false);
    void start() override;
    void stop() override;

public:
    static constexpr uint32_t HEADER = 0x30764151; // QAv0

    Filesystem(const Filesystem&) = delete;
    Filesystem& operator = (const Filesystem&) = delete;

    static SharedThread<Filesystem> createTemporaryFilesystem();

    fs::path getRootDir() const
    {
        return m_root;
    }

    fs::path getDatabaseDir() const
    {
        return m_database;
    }

    bool isTemporary() const
    {
        return m_temporary;
    }

    Serializer_ptr read(const std::filesystem::path& path);
    std::shared_ptr<json> readJSON(const std::filesystem::path& path);

    void read(const std::filesystem::path& path, SerializerCallback&& callback);
    void write(const std::filesystem::path& path, const Serializer_ptr& serializer);

private:
    fs::path m_root;
    bool m_temporary;

    mutable Logger m_logger;

    fs::path m_database;
};

}
