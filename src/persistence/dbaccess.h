// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PERSIST_DB_ACCESS_H
#define PERSIST_DB_ACCESS_H

#include "commons/uint256.h"
#include "dbconf.h"
#include "leveldbwrapper.h"

#include <string>
#include <tuple>
#include <vector>
#include <optional>

using namespace std;

/**
 * Empty functions
 */
namespace db_util {

#define DEFINE_NUMERIC_EMPTY(type) \
    inline bool IsEmpty(const type val) { return val == 0; } \
    inline void SetEmpty(type &val) { val = 0; }

    // bool
    inline bool IsEmpty(const bool val) { return val == false; }
    inline void SetEmpty(bool &val) { val = false; }



    DEFINE_NUMERIC_EMPTY(int32_t)
    DEFINE_NUMERIC_EMPTY(uint8_t)
    DEFINE_NUMERIC_EMPTY(uint16_t)
    DEFINE_NUMERIC_EMPTY(uint32_t)
    DEFINE_NUMERIC_EMPTY(uint64_t)

    // string
    template<typename C> bool IsEmpty(const basic_string<C> &val);
    template<typename C> void SetEmpty(basic_string<C> &val);

    // vector
    template<typename T, typename A> bool IsEmpty(const vector<T, A>& val);
    template<typename T, typename A> void SetEmpty(vector<T, A>& val);

    //optional
    template<typename T> bool IsEmpty(const std::optional<T> val) { return val == std::nullopt; }
    template<typename T> void SetEmpty(std::optional<T>& val) { val = std::nullopt; }

    // set
    template<typename K, typename Pred, typename A> bool IsEmpty(const set<K, Pred, A>& val);
    template<typename K, typename Pred, typename A> void SetEmpty(set<K, Pred, A>& val);

    // 2 pair
    template<typename K, typename T> bool IsEmpty(const std::pair<K, T>& val);
    template<typename K, typename T> void SetEmpty(std::pair<K, T>& val);

    // 3 tuple
    template<typename T0, typename T1, typename T2> bool IsEmpty(const std::tuple<T0, T1, T2>& val);
    template<typename T0, typename T1, typename T2> void SetEmpty(std::tuple<T0, T1, T2>& val);

    // common Object Type, must support T.IsEmpty() and T.SetEmpty()
    template<typename T> bool IsEmpty(const T& val);
    template<typename T> void SetEmpty(T& val);

    // string
    template<typename C>
    bool IsEmpty(const basic_string<C> &val) {
        return val.empty();
    }

    template<typename C>
    void SetEmpty(basic_string<C> &val) {
        val.clear();
    }

    // vector
    template<typename T, typename A>
    bool IsEmpty(const vector<T, A>& val) {
        return val.empty();
    }
    template<typename T, typename A>
    void SetEmpty(vector<T, A>& val) {
        val.clear();
    }

    // set
    template<typename K, typename Pred, typename A> bool IsEmpty(const set<K, Pred, A>& val) {
        return val.empty();
    }
    template<typename K, typename Pred, typename A> void SetEmpty(set<K, Pred, A>& val) {
        val.clear();
    }

    // 2 pair
    template<typename K, typename T>
    bool IsEmpty(const std::pair<K, T>& val) {
        return IsEmpty(val.first) && IsEmpty(val.second);
    }
    template<typename K, typename T>
    void SetEmpty(std::pair<K, T>& val) {
        SetEmpty(val.first);
        SetEmpty(val.second);
    }

    // 3 tuple
    template<typename T0, typename T1, typename T2>
    bool IsEmpty(const std::tuple<T0, T1, T2>& val) {
        return IsEmpty(std::get<0>(val)) &&
               IsEmpty(std::get<1>(val)) &&
               IsEmpty(std::get<2>(val));
    }
    template<typename T0, typename T1, typename T2>
    void SetEmpty(std::tuple<T0, T1, T2>& val) {
        SetEmpty(std::get<0>(val));
        SetEmpty(std::get<1>(val));
        SetEmpty(std::get<2>(val));
    }

    // common Object Type, must support T.IsEmpty() and T.SetEmpty()
    template<typename T>
    bool IsEmpty(const T& val) {
        return val.IsEmpty();
    }

    template<typename T>
    void SetEmpty(T& val) {
        val.SetEmpty();
    }

    template <typename ValueType>
    std::shared_ptr<ValueType> MakeEmptyValue() {
        auto value = std::make_shared<ValueType>();
        SetEmpty(*value);
        return value;
    }

    template<typename T>
    T MakeEmpty() {
        T value; SetEmpty(value);
        return value;
    }
};

typedef void(UndoDataFunc)(const CDbOpLogs &pDbOpLogs);
typedef std::map<dbk::PrefixType, std::function<UndoDataFunc>> UndoDataFuncMap;

class CDBAccess {
public:
    CDBAccess(const boost::filesystem::path& dir, DBNameType dbNameTypeIn, bool fMemory, bool fWipe) :
              dbNameType(dbNameTypeIn),
              db( dir / ::GetDbName(dbNameTypeIn), DBCacheSize[dbNameTypeIn], fMemory, fWipe ) {}

    int64_t GetDbCount() const { return db.GetDbCount(); }
    template<typename KeyType, typename ValueType>
    bool GetData(const dbk::PrefixType prefixType, const KeyType &key, ValueType &value) const {
        string keyStr = dbk::GenDbKey(prefixType, key);
        return db.Read(keyStr, value);
    }

    template<typename ValueType>
    bool GetData(const dbk::PrefixType prefixType, ValueType &value) const {
        const string prefix = dbk::GetKeyPrefix(prefixType);
        return db.Read(prefix, value);
    }

    template <typename KeyType>
    bool GetTopNElements(const uint32_t maxNum, const dbk::PrefixType prefixType, set<KeyType> &expiredKeys,
                         set<KeyType> &keys) {
        KeyType key;
        uint32_t count             = 0;
        shared_ptr<leveldb::Iterator> pCursor = NewIterator();

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        const string &prefix = dbk::GetKeyPrefix(prefixType);
        ssKey.write(prefix.c_str(), prefix.size());
        pCursor->Seek(ssKey.str());

        for (; (count < maxNum) && pCursor->Valid(); pCursor->Next()) {
            boost::this_thread::interruption_point();

            try {
                leveldb::Slice slKey = pCursor->key();
                if (!dbk::ParseDbKey(slKey, prefixType, key)) {
                    break;
                }

                if (expiredKeys.count(key)) {
                    continue;
                } else if (keys.count(key)) {
                    // skip it if the element existed in memory cache(upper level cache)
                    continue;
                } else {
                    // Got an valid element.
                    auto ret = keys.emplace(key);
                    if (!ret.second)
                        throw runtime_error(strprintf("%s :  %s, alloc new cache item failed", __FUNCTION__, __LINE__));

                    ++count;
                }
            } catch (std::exception &e) {
                return ERRORMSG("%s : Deserialize or I/O error - %s", __FUNCTION__, e.what());
            }
        }

        return true;
    }

    template <typename KeyType, typename ValueType>
    bool GetAllElements(const dbk::PrefixType prefixType, map<KeyType, ValueType> &elements) {
        KeyType key;
        ValueType value;
        shared_ptr<leveldb::Iterator> pCursor = NewIterator();

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        const string &prefix = dbk::GetKeyPrefix(prefixType);
        ssKey.write(prefix.c_str(), prefix.size());
        pCursor->Seek(ssKey.str());

        for (; pCursor->Valid(); pCursor->Next()) {
            boost::this_thread::interruption_point();

            try {
                const auto &slKey = pCursor->key();
                if (!dbk::ParseDbKey(slKey, prefixType, key)) {
                    break;
                }

                // Got an valid element.
                const auto &slValue = pCursor->value();
                CDataStream ds(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                ds >> value;
                auto ret = elements.emplace(key, value);
                if (!ret.second)
                    throw runtime_error(strprintf("%s :  %s, alloc new cache item failed", __FUNCTION__, __LINE__));
            } catch (std::exception &e) {
                return ERRORMSG("%s : Deserialize or I/O error - %s", __FUNCTION__, e.what());
            }
        }

        return true;
    }

    // GetAllElements
    // support input end key
    template <typename KeyType, typename ValueType>
    bool GetAllElements(const dbk::PrefixType prefixType, const KeyType &endKey,
                        map<KeyType, ValueType> &elements, set<KeyType> &expiredKeys) {

        KeyType key;
        ValueType value;
        shared_ptr<leveldb::Iterator> pCursor = NewIterator();
        const string &prefixStr = dbk::GetKeyPrefix(prefixType);

        for (pCursor->Seek(prefixStr); pCursor->Valid(); pCursor->Next()) {
            boost::this_thread::interruption_point();

            try {
                leveldb::Slice slKey = pCursor->key();
                if (!dbk::ParseDbKey(slKey, prefixType, key)) { // the rest key is other prefix type
                    break;
                }

                if (endKey < key)
                    break;

                if (expiredKeys.count(key) || elements.count(key)) // key exists in cache
                    continue;

                // Got an valid element.
                leveldb::Slice slValue = pCursor->value();
                CDataStream ds(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                ds >> value;
                auto ret = elements.emplace(key, value);
                if (!ret.second)
                    throw runtime_error(strprintf("%s :  %s, alloc new cache item failed", __FUNCTION__, __LINE__));

            } catch (std::exception &e) {
                return ERRORMSG("%s : Deserialize or I/O error - %s", __FUNCTION__, e.what());
            }
        }

        return true;
    }

    template <typename KeyType, typename ValueType>
    bool GetAllElements(const dbk::PrefixType prefixType, set<KeyType> &expiredKeys,
                        map<KeyType, ValueType> &elements) {
        KeyType key;
        ValueType value;
        shared_ptr<leveldb::Iterator> pCursor = NewIterator();
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        const string &prefix = dbk::GetKeyPrefix(prefixType);
        ssKey.write(prefix.c_str(), prefix.size());
        pCursor->Seek(ssKey.str());

        for (; pCursor->Valid(); pCursor->Next()) {
            boost::this_thread::interruption_point();

            try {
                leveldb::Slice slKey = pCursor->key();
                if (!dbk::ParseDbKey(slKey, prefixType, key)) {
                    break;
                }

                if (expiredKeys.count(key)) {
                    continue;
                } else if (elements.count(key)) {
                    // skip it if the element existed in memory cache(upper level cache)
                    continue;
                } else {
                    // Got an valid element.
                    leveldb::Slice slValue = pCursor->value();
                    CDataStream ds(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                    ds >> value;
                    auto ret = elements.emplace(key, value);
                    if (!ret.second)
                        throw runtime_error(strprintf("%s :  %s, alloc new cache item failed", __FUNCTION__, __LINE__));
                }
            } catch (std::exception &e) {
                return ERRORMSG("%s : Deserialize or I/O error - %s", __FUNCTION__, e.what());
            }
        }

        return true;
    }

    template<typename KeyType, typename ValueType>
    bool HaveData(const dbk::PrefixType prefixType, const KeyType &key) const {
        string keyStr = dbk::GenDbKey(prefixType, key);
        return db.Exists(keyStr);
    }

    template<typename KeyType, typename ValueType>
    void BatchWrite(const dbk::PrefixType prefixType, const map<KeyType, ValueType> &mapData) {        CLevelDBBatch batch;
        for (auto item : mapData) {
            string key = dbk::GenDbKey(prefixType, item.first);
            if (db_util::IsEmpty(item.second)) {
                batch.Erase(key);
            } else {
                batch.Write(key, item.second);
            }
        }
        db.WriteBatch(batch, true);
    }

    template<typename ValueType>
    void BatchWrite(const dbk::PrefixType prefixType, ValueType &value) {
        CLevelDBBatch batch;
        const string prefix = dbk::GetKeyPrefix(prefixType);

        if (db_util::IsEmpty(value)) {
            batch.Erase(prefix);
        } else {
            batch.Write(prefix, value);
        }
        db.WriteBatch(batch, true);
    }

    DBNameType GetDbNameType() const { return dbNameType; }

    std::shared_ptr<leveldb::Iterator> NewIterator() {
        return std::shared_ptr<leveldb::Iterator>(db.NewIterator());
    }
private:
    DBNameType dbNameType;
    mutable CLevelDBWrapper db; // // TODO: remove the mutable declare
};

template<int32_t PREFIX_TYPE_VALUE, typename __KeyType, typename __ValueType>
class CCompositeKVCache {
public:
    static const dbk::PrefixType PREFIX_TYPE = (dbk::PrefixType)PREFIX_TYPE_VALUE;
public:
    typedef __KeyType   KeyType;
    typedef __ValueType ValueType;
    typedef typename std::map<KeyType, ValueType> Map;
    typedef typename std::map<KeyType, ValueType>::iterator Iterator;

public:
    /**
     * Default constructor, must use set base to initialize before using.
     */
    CCompositeKVCache(): pBase(nullptr), pDbAccess(nullptr) {};

    CCompositeKVCache(CCompositeKVCache *pBaseIn): pBase(pBaseIn),
        pDbAccess(nullptr) {
        assert(pBaseIn != nullptr);
    };

    CCompositeKVCache(CDBAccess *pDbAccessIn): pBase(nullptr),
        pDbAccess(pDbAccessIn), is_calc_size(true) {
        assert(pDbAccessIn != nullptr);
        assert(pDbAccess->GetDbNameType() == GetDbNameEnumByPrefix(PREFIX_TYPE));
    };

    void SetBase(CCompositeKVCache *pBaseIn) {
        assert(pDbAccess == nullptr);
        assert(mapData.empty());
        pBase = pBaseIn;
    };

    void SetDbOpLogMap(CDBOpLogMap *pDbOpLogMapIn) {
        pDbOpLogMap = pDbOpLogMapIn;
    }

    bool IsCalcSize() const { return is_calc_size; }

    uint32_t GetCacheSize() const {
        return size;
    }

    bool GetTopNElements(const uint32_t maxNum, set<KeyType> &keys) {
        // 1. Get all candidate elements.
        set<KeyType> expiredKeys;
        set<KeyType> candidateKeys;
        if (!GetTopNElements(maxNum, expiredKeys, candidateKeys)) {
            // TODO: log
            return false;
        }

        // 2. Get the top N elements.
        uint32_t count  = 0;
        for (const auto item : candidateKeys) {
            if (count ++ == maxNum) {
                break;
            }
            keys.emplace(item);
        }

        return keys.size() == maxNum;
    }

    // map<string, ValueType>
    bool GetAllElements(const KeyType &endKey, Map &elements) {
        set<KeyType> expiredKeys;
        if (!GetAllElements(endKey, elements, expiredKeys)) {
            // TODO: log
            return false;
        }

        return true;
    }

    bool GetAllElements(map<KeyType, ValueType> &elements) {
        set<KeyType> expiredKeys;
        if (!GetAllElements(expiredKeys, elements)) {
            // TODO: log
            return false;
        }

        return true;
    }

    bool GetData(const KeyType &key, ValueType &value) const {
        if (db_util::IsEmpty(key)) {
            return false;
        }
        auto it = GetDataIt(key);
        if (it != mapData.end() && !db_util::IsEmpty(it->second)) {
            value = it->second;
            return true;
        }
        return false;
    }

    bool SetData(const KeyType &key, const ValueType &value) {
        if (db_util::IsEmpty(key)) {
            return false;
        }
        auto it = GetDataIt(key);
        if (it == mapData.end()) {
            AddOpLog(key, nullptr);
            AddDataToMap(key, value);
        } else {
            AddOpLog(key, &it->second);
            UpdateDataSize(it->second, value);
            it->second = value;
        }
        return true;
    }

    bool HaveData(const KeyType &key) const {
        if (db_util::IsEmpty(key)) {
            return false;
        }
        auto it = GetDataIt(key);
        return it != mapData.end() && !db_util::IsEmpty(it->second);
    }

    bool EraseData(const KeyType &key) {
        if (db_util::IsEmpty(key)) {
            return false;
        }
        Iterator it = GetDataIt(key);
        if (it != mapData.end() && !db_util::IsEmpty(it->second)) {
            DecDataSize(it->second);
            AddOpLog(key, &it->second);
            db_util::SetEmpty(it->second);
            IncDataSize(it->second);
        }
        return true;
    }

    void Clear() {
        mapData.clear();
        size = 0;
    }

    void Flush() {
        assert(pBase != nullptr || pDbAccess != nullptr);
        if (pBase != nullptr) {
            assert(pDbAccess == nullptr);
            for (auto it : mapData) {
                pBase->mapData[it.first] = it.second;
            }
        } else if (pDbAccess != nullptr) {
            assert(pBase == nullptr);
            pDbAccess->BatchWrite<KeyType, ValueType>(PREFIX_TYPE, mapData);
        }

        Clear();
    }

    void UndoData(const CDbOpLog &dbOpLog) {
        KeyType key;
        ValueType value;
        dbOpLog.Get(key, value);
        auto it = mapData.find(key);
        if (it != mapData.end()) {
            UpdateDataSize(it->second, value);
            it->second = value;
        } else {
            AddDataToMap(key, value);
        }
    }

    void UndoDataList(const CDbOpLogs &dbOpLogs) {
        for (auto it = dbOpLogs.rbegin(); it != dbOpLogs.rend(); it++) {
            UndoData(*it);
        }
    }

    void RegisterUndoFunc(UndoDataFuncMap &undoDataFuncMap) {
        undoDataFuncMap[GetPrefixType()] = std::bind(&CCompositeKVCache::UndoDataList, this, std::placeholders::_1);
    }

    dbk::PrefixType GetPrefixType() const { return PREFIX_TYPE; }

    CDBAccess* GetDbAccessPtr() {
        CDBAccess* pRet = pDbAccess;
        if (pRet == nullptr && pBase != nullptr) {
            pRet = pBase->GetDbAccessPtr();
        }
        return pRet;
    }

    CCompositeKVCache<PREFIX_TYPE, KeyType, ValueType>* GetBasePtr() { return pBase; }

    map<KeyType, ValueType>& GetMapData() { return mapData; };
private:
    Iterator GetDataIt(const KeyType &key) const {
        Iterator it = mapData.find(key);
        if (it != mapData.end()) {
            return it;
        } else if (pBase != nullptr) {
            // find key-value at base cache
            auto baseIt = pBase->GetDataIt(key);
            if (baseIt != pBase->mapData.end()) {
                // the found key-value add to current mapData
                return AddDataToMap(key, baseIt->second);
            }
        } else if (pDbAccess != NULL) {
            // TODO: need to save the empty value to mapData for search performance?
            auto pDbValue = db_util::MakeEmptyValue<ValueType>();
            if (pDbAccess->GetData(PREFIX_TYPE, key, *pDbValue)) {
                return AddDataToMap(key, *pDbValue);
            }
        }

        return mapData.end();
    }

    inline Iterator AddDataToMap(const KeyType &keyIn, const ValueType &valueIn) const {
        auto newRet = mapData.emplace(keyIn, valueIn);
        if (!newRet.second)
            throw runtime_error(strprintf("%s :  %s, alloc new cache item failed", __FUNCTION__, __LINE__));
        IncDataSize(keyIn, valueIn);
        return newRet.first;
    }

    inline void IncDataSize(const KeyType &keyIn, const ValueType &valueIn) const {
        if (is_calc_size) {
            size += CalcDataSize(keyIn);
            size += CalcDataSize(valueIn);
        }
    }

    inline void IncDataSize(const ValueType &valueIn) const {
        if (is_calc_size)
            size += CalcDataSize(valueIn);
    }

    inline void DecDataSize(const ValueType &valueIn) const {
        if (is_calc_size) {
            uint32_t sz = CalcDataSize(valueIn);
            size = size > sz ? size - sz : 0;
        }
    }

    inline void UpdateDataSize(const ValueType &oldValue, const ValueType &newVvalue) const {
        if (is_calc_size) {
            size += CalcDataSize(newVvalue);
            uint32_t oldSz = CalcDataSize(oldValue);
            size = size > oldSz ? size - oldSz : 0;
        }
    }

    template <typename Data>
    inline uint32_t CalcDataSize(const Data &d) const {
        return ::GetSerializeSize(d, SER_DISK, CLIENT_VERSION);
    }

    bool GetTopNElements(const uint32_t maxNum, set<KeyType> &expiredKeys, set<KeyType> &keys) {
        if (!mapData.empty()) {
            uint32_t count = 0;
            auto iter      = mapData.begin();

            for (; (count < maxNum) && iter != mapData.end(); ++iter) {
                if (db_util::IsEmpty(iter->second)) {
                    expiredKeys.insert(iter->first);
                } else if (expiredKeys.count(iter->first) || keys.count(iter->first)) {
                    // TODO: log
                    continue;
                } else {
                    // Got a valid element.
                    keys.insert(iter->first);

                    ++count;
                }
            }
        }

        if (pBase != nullptr) {
            return pBase->GetTopNElements(maxNum, expiredKeys, keys);
        } else if (pDbAccess != nullptr) {
            return pDbAccess->GetTopNElements(maxNum, PREFIX_TYPE, expiredKeys, keys);
        }

        return true;
    }

    // map<string, ValueType>
    bool GetAllElements(const KeyType &endKey, Map &mapDataOut, set<KeyType> &expiredKeys) {
        if (!mapData.empty()) {
            for (auto iter = mapData.begin(); iter != mapData.end() && iter->first < endKey; iter++) {
                if (!expiredKeys.count(iter->first) && !mapDataOut.count(iter->first)) { // check not got
                    if (db_util::IsEmpty(iter->second)) { // empty, will be deleted
                        expiredKeys.insert(iter->first);
                    } else { // Got a valid element.
                        mapDataOut.emplace(iter->first, iter->second);
                    }
                }
            }
        }

        if (pBase != nullptr) {
            return pBase->GetAllElements(endKey, mapDataOut, expiredKeys);
        } else if (pDbAccess != nullptr) {
            return pDbAccess->GetAllElements(PREFIX_TYPE, endKey, mapDataOut, expiredKeys);
        }

        return true;
    }

    bool GetAllElements(set<KeyType> &expiredKeys, map<KeyType, ValueType> &elements) {
        if (!mapData.empty()) {
            for (auto iter : mapData) {
                if (db_util::IsEmpty(iter.second)) {
                    expiredKeys.insert(iter.first);
                } else if (expiredKeys.count(iter.first) || elements.count(iter.first)) {
                    // TODO: log
                    continue;
                } else {
                    // Got a valid element.
                    elements.insert(iter);
                }
            }
        }

        if (pBase != nullptr) {
            return pBase->GetAllElements(expiredKeys, elements);
        } else if (pDbAccess != nullptr) {
            return pDbAccess->GetAllElements(PREFIX_TYPE, expiredKeys, elements);
        }

        return true;
    }

    inline void AddOpLog(const KeyType &key, const ValueType *pOldValue) {
        if (pDbOpLogMap != nullptr) {
            CDbOpLog dbOpLog;
            if (pOldValue != nullptr)
                dbOpLog.Set(key, *pOldValue);
            else { // new data
                auto pEmptyValue = db_util::MakeEmptyValue<ValueType>();
                dbOpLog.Set(key, *pEmptyValue);
            }
            pDbOpLogMap->AddOpLog(PREFIX_TYPE, dbOpLog);
        }

    }
private:
    mutable CCompositeKVCache<PREFIX_TYPE, KeyType, ValueType> *pBase = nullptr;
    CDBAccess *pDbAccess = nullptr;
    mutable map<KeyType, ValueType> mapData;
    CDBOpLogMap *pDbOpLogMap = nullptr;
    bool is_calc_size = false;
    mutable uint32_t size = 0;
};


template<int32_t PREFIX_TYPE_VALUE, typename __ValueType>
class CSimpleKVCache {
public:
    typedef __ValueType ValueType;
    static const dbk::PrefixType PREFIX_TYPE = (dbk::PrefixType)PREFIX_TYPE_VALUE;
public:
    /**
     * Default constructor, must use set base to initialize before using.
     */
    CSimpleKVCache(): pBase(nullptr), pDbAccess(nullptr) {};

    CSimpleKVCache(CSimpleKVCache *pBaseIn): pBase(pBaseIn),
        pDbAccess(nullptr) {
        assert(pBaseIn != nullptr);
    }

    CSimpleKVCache(CDBAccess *pDbAccessIn): pBase(nullptr),
        pDbAccess(pDbAccessIn) {
        assert(pDbAccessIn != nullptr);
    }

    CSimpleKVCache(const CSimpleKVCache &other) {
        operator=(other);
    }

    CSimpleKVCache& operator=(const CSimpleKVCache& other) {
        pBase = other.pBase;
        pDbAccess = other.pDbAccess;
        // deep copy for shared_ptr
        if (other.ptrData == nullptr) {
            ptrData = nullptr;
        } else {
            ptrData = make_shared<ValueType>(*other.ptrData);
        }
        pDbOpLogMap = other.pDbOpLogMap;
        return *this;
    }

    void SetBase(CSimpleKVCache *pBaseIn) {
        assert(pDbAccess == nullptr);
        assert(!ptrData && "Must SetBase before have any data");
        pBase = pBaseIn;
    }

    void SetDbOpLogMap(CDBOpLogMap *pDbOpLogMapIn) {
        pDbOpLogMap = pDbOpLogMapIn;
    }

    uint32_t GetCacheSize() const {
        if (!ptrData) {
            return 0;
        }

        return ::GetSerializeSize(*ptrData, SER_DISK, CLIENT_VERSION);
    }

    bool GetData(ValueType &value) const {
        auto ptr = GetDataPtr();
        if (ptr && !db_util::IsEmpty(*ptr)) {
            value = *ptr;
            return true;
        }
        return false;
    }

    bool SetData(const ValueType &value) {
        if (!ptrData) {
            ptrData = db_util::MakeEmptyValue<ValueType>();
        }
        AddOpLog(*ptrData);
        *ptrData = value;
        return true;
    }

    bool HaveData() const {
        auto ptr = GetDataPtr();
        return ptr && !db_util::IsEmpty(*ptr);
    }

    bool EraseData() {
        auto ptr = GetDataPtr();
        if (ptr && !db_util::IsEmpty(*ptr)) {
            AddOpLog(*ptr);
            db_util::SetEmpty(*ptr);
        }
        return true;
    }

    void Clear() {
        ptrData = nullptr;
    }

    void Flush() {
        assert(pBase != nullptr || pDbAccess != nullptr);
        if (ptrData) {
            if (pBase != nullptr) {
                assert(pDbAccess == nullptr);
                pBase->ptrData = ptrData;
            } else if (pDbAccess != nullptr) {
                assert(pBase == nullptr);
                pDbAccess->BatchWrite(PREFIX_TYPE, *ptrData);
            }
            ptrData = nullptr;
        }
    }

    void UndoData(const CDbOpLog &dbOpLog) {
        if (!ptrData) {
            ptrData = db_util::MakeEmptyValue<ValueType>();
        }
        dbOpLog.Get(*ptrData);
    }

    void UndoDataList(const CDbOpLogs &dbOpLogs) {
        for (auto it = dbOpLogs.rbegin(); it != dbOpLogs.rend(); it++) {
            UndoData(*it);
        }
    }

    void RegisterUndoFunc(UndoDataFuncMap &undoDataFuncMap) {
        undoDataFuncMap[GetPrefixType()] = std::bind(&CSimpleKVCache::UndoDataList, this, std::placeholders::_1);
    }

    dbk::PrefixType GetPrefixType() const { return PREFIX_TYPE; }
private:
    std::shared_ptr<ValueType> GetDataPtr() const {

        if (ptrData) {
            return ptrData;
        } else if (pBase != nullptr){
            auto ptr = pBase->GetDataPtr();
            if (ptr) {
                ptrData = std::make_shared<ValueType>(*ptr);
                return ptrData;
            }
        } else if (pDbAccess != NULL) {
            auto ptrDbData = db_util::MakeEmptyValue<ValueType>();

            if (pDbAccess->GetData(PREFIX_TYPE, *ptrDbData)) {
                assert(!db_util::IsEmpty(*ptrDbData));
                ptrData = ptrDbData;
                return ptrData;
            }
        }
        return nullptr;
    }

    inline void AddOpLog(const ValueType &oldValue) {
        if (pDbOpLogMap != nullptr) {
            CDbOpLog dbOpLog;
            dbOpLog.Set(oldValue);
            pDbOpLogMap->AddOpLog(PREFIX_TYPE, dbOpLog);
        }

    }
private:
    mutable CSimpleKVCache<PREFIX_TYPE, ValueType> *pBase;
    CDBAccess *pDbAccess;
    mutable std::shared_ptr<ValueType> ptrData = nullptr;
    CDBOpLogMap *pDbOpLogMap                   = nullptr;
};

#endif  // PERSIST_DB_ACCESS_H
