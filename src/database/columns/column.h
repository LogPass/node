#pragma once

namespace logpass {
namespace database {

// base class for database columns
class Column {
public:
    Column(rocksdb::DB* db, rocksdb::ColumnFamilyHandle* handle) : m_db(db), m_handle(handle) {}
    virtual ~Column() = default;
    Column(const Column&) = delete;
    Column& operator = (const Column&) = delete;

    static rocksdb::ColumnFamilyOptions getOptions();

    // loads column state from database, should be used when column is initialized or rollbacked
    virtual void load() = 0;
    // preloads data to be used later
    virtual void preload(uint32_t blockId) {};
    // prepares commit, write changes to write batch
    virtual void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) = 0;
    // commits changes
    virtual void commit() = 0;
    // clears temporary values
    virtual void clear() = 0;

    rocksdb::ColumnFamilyHandle* getHandle()
    {
        return m_handle;
    }

    rocksdb::ColumnFamilyMetaData getMetaData() const
    {
        rocksdb::ColumnFamilyMetaData meta;
        m_db->GetColumnFamilyMetaData(m_handle, &meta);
        return meta;
    }

    uint32_t getBlockId() const
    {
        auto s = get(rocksdb::Slice());
        if (!s) {
            return 0;
        }
        s->get<uint8_t>(); // version
        return s->get<uint32_t>();
    }

    std::string getName() const
    {
        return getMetaData().name;
    }

protected:
    class AppendMergeOperator : public rocksdb::AssociativeMergeOperator {
        bool Merge(const rocksdb::Slice& key, const rocksdb::Slice* existing_value,
                   const rocksdb::Slice& value, std::string* new_value,
                   rocksdb::Logger* logger) const override
        {
            *new_value = (existing_value ? existing_value->ToString() : ""s) + value.ToString();
            return true;
        };

        const char* Name() const override
        {
            return "AppendMergeOperator";
        }
    };

    // returns value from database, may return nullptr
    Serializer_ptr get(const rocksdb::Slice& key) const
    {
        std::string value;
        auto status = m_db->Get(rocksdb::ReadOptions(), m_handle, key, &value);
        if (!status.ok()) {
            return nullptr;
        }
        return std::make_shared<Serializer>(value);
    }

    // returns multiple values from database
    std::vector<Serializer_ptr> multiGet(const std::vector<rocksdb::Slice>& keys) const
    {
        std::vector<std::string> values;
        auto statuses = m_db->MultiGet(rocksdb::ReadOptions(),
                                       std::vector<rocksdb::ColumnFamilyHandle*>(keys.size(), m_handle), keys, &values);
        std::vector<Serializer_ptr> serializers(values.size(), nullptr);
        for (size_t i = 0; i < statuses.size(); ++i) {
            if (statuses[i].ok()) {
                serializers[i] = std::make_shared<Serializer>(values[i]);
            }
        }
        return serializers;
    }

    // returns value from database, may return nullptr
    Serializer_ptr get(Serializer& key) const
    {
        return get((rocksdb::Slice)key);
    }

    // returns value from database, may return nullptr
    template<typename K>
    Serializer_ptr get(const K& key) const
    {
        Serializer s;
        s(key);
        return get(s);
    };

    // returns multi value from database, may return nullptr
    template<typename K>
    std::vector<Serializer_ptr> multiGet(const std::vector<K>& keys) const
    {
        std::vector<rocksdb::Slice> slices(keys.size());
        for (size_t i = 0; i < keys.size(); ++i) {
            slices[i] = keys[i];
        }
        return multiGet(slices);
    };

    // returns value from database, may return nullptr
    template<typename K1, typename K2>
    Serializer_ptr get(const K1& key1, const K2& key2) const
    {
        Serializer s;
        s(key1);
        s(key2);
        return get(s);
    };

    // returns value from database, may return nullptr
    template<typename K1, typename K2, typename K3>
    Serializer_ptr get(const K1& key1, const K2& key2, const K2& key3) const
    {
        Serializer s;
        s(key1);
        s(key2);
        s(key3);
        return get(s);
    };

    // puts value in batch
    void put(rocksdb::WriteBatch& batch, const rocksdb::Slice& key, const rocksdb::Slice& value) const
    {
        batch.Put(m_handle, key, value);
    }

    // puts value in batch
    void put(rocksdb::WriteBatch& batch, Serializer& key, const rocksdb::Slice& value) const
    {
        put(batch, (rocksdb::Slice)key, value);
    }

    // puts value in batch
    template<typename K>
    void put(rocksdb::WriteBatch& batch, const K& key, const rocksdb::Slice& value) const
    {
        Serializer s;
        s(key);
        put(batch, s, value);
    };

    // puts value in batch
    template<typename K1, typename K2>
    void put(rocksdb::WriteBatch& batch, const K1& key1, const K2& key2, const rocksdb::Slice& value) const
    {
        Serializer s;
        s(key1);
        s(key2);
        put(batch, s, value);
    };

    // puts value in batch
    template<typename K1, typename K2, typename K3>
    void put(rocksdb::WriteBatch& batch, const K1& key1, const K2& key2, const K2& key3,
             const rocksdb::Slice& value) const
    {
        Serializer s;
        s(key1);
        s(key2);
        s(key3);
        put(batch, s, value);
    };

    rocksdb::DB* m_db;
    rocksdb::ColumnFamilyHandle* m_handle;
    mutable std::shared_mutex m_mutex;
};

}
}
