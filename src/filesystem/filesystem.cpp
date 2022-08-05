#include "pch.h"
#include "filesystem.h"

namespace logpass {

Filesystem::Filesystem(const fs::path& root, bool temporary) :
    EventLoopThread("filesystem"), m_root(root), m_temporary(temporary)
{
    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("Filesystem"));
    m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(root.string()));
}

void Filesystem::start()
{
    if (!fs::is_directory(m_root)) {
        LOG_CLASS(fatal) << "Invalid blockchain directory: " << m_root.string();
        std::terminate();
    }

    m_database = m_root / "database";

    for (auto& dir : { m_database }) {
        try {
            if (fs::is_directory(dir))
                continue;
            if (!fs::create_directory(dir) || !fs::is_directory(dir)) {
                THROW_EXCEPTION(std::runtime_error("unknown error"));
            }
        } catch (const std::exception& ec) {
            LOG_CLASS(fatal) << "Can't create directory directory " << dir.string() <<
                " (" << ec.what() << ")";
            std::terminate();
        }
    }

    EventLoopThread::start();
}

void Filesystem::stop()
{
    EventLoopThread::stop();
    if (m_temporary) {
        fs::remove_all(m_root);
    }
}

SharedThread<Filesystem> Filesystem::createTemporaryFilesystem()
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::shuffle(str.begin(), str.end(), generator);
    std::string tempDirName = "tmp_"s + str.substr(0, 16);

    fs::path tmpPath = fs::temp_directory_path();
    tmpPath /= "logpass";
    tmpPath /= tempDirName;

    fs::create_directories(tmpPath);

    return SharedThread<Filesystem>(tmpPath, true);
}

Serializer_ptr Filesystem::read(const std::filesystem::path& path)
{
    std::promise<Serializer_ptr> promise;
    read(path, (SerializerCallback)[&](Serializer_ptr serializer) {
        promise.set_value(serializer);
    });
    return promise.get_future().get();
}

std::shared_ptr<json> Filesystem::readJSON(const std::filesystem::path& path)
{
    auto s = read(path);
    if (!s) {
        return nullptr;
    }
    try {
        return std::make_shared<json>(json::parse(s->buffer(), s->buffer() + s->size()));
    } catch (const json::exception& e) {
        LOG_CLASS(error) << "Can't parse JSON from file: " << path.string() << ", error: " << e.what();
    }
    return nullptr;
}

void Filesystem::read(const std::filesystem::path& path, SerializerCallback&& callback)
{
    post([this, path, callback = std::move(callback)]{
        try {
           std::ifstream fs(m_root / path, std::ifstream::binary);
           if (!fs) {
               LOG_CLASS(error) << "Can't open: " << path.string();
               return callback(nullptr);
           }
           return callback(std::make_shared<Serializer>(fs, path.string()));
        } catch (const std::exception& e) {
            LOG_CLASS(error) << "Can't open: " << path.string() << ", error: " << e.what();
        }
        return callback(nullptr);
    });
}

void Filesystem::write(const std::filesystem::path& path, const Serializer_ptr& serializer)
{
    post([this, path, serializer] {
        auto parentPath = path.parent_path();
        try {
            if (!fs::is_directory(m_root / parentPath)) {
                fs::create_directories(m_root / parentPath);
            }
            std::ofstream fs(m_root / path, std::ofstream::binary);
            if (!fs) {
                LOG_CLASS(error) << "Can't write to to " << path.string();
                return;
            }
            serializer->writeTo(fs);
        } catch (const std::exception& e) {
            LOG_CLASS(error) << "Can't write to: " << path.string() << ", error: " << e.what();
        }
    });
}

}
