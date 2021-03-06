// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PERSIST_CDPDB_H
#define PERSIST_CDPDB_H

#include "commons/uint256.h"
#include "entities/cdp.h"
#include "dbaccess.h"

#include <map>
#include <set>
#include <string>
#include <cstdint>

using namespace std;

/*  CCompositeKVCache     prefixType        key                  value           variable  */
/*  ----------------   --------------      -----------------    --------------   -----------*/
// cdpr{$Ratio}{$height}{$cdpid} -> CUserCDP
typedef CCompositeKVCache<dbk::CDP_RATIO, tuple<string, string, uint256>, CUserCDP>      RatioCDPIdCache;



class CCdpDBCache {
public:
    CCdpDBCache() {}
    CCdpDBCache(CDBAccess *pDbAccess);
    CCdpDBCache(CCdpDBCache *pBaseIn);

    bool NewCDP(const int32_t blockHeight, CUserCDP &cdp);
    bool EraseCDP(const CUserCDP &oldCDP, const CUserCDP &cdp);
    bool UpdateCDP(const CUserCDP &oldCDP, const CUserCDP &newCDP);

    bool GetCDPList(const CRegID &regId, vector<CUserCDP> &cdpList);
    bool GetCDP(const uint256 cdpid, CUserCDP &cdp);

    bool GetCdpListByCollateralRatio(const uint64_t collateralRatio, const uint64_t bcoinMedianPrice,
                                     RatioCDPIdCache::Map &userCdps);

    inline uint64_t GetGlobalStakedBcoins() const;
    inline uint64_t GetGlobalOwedScoins() const;
    void GetGlobalItem(uint64_t &globalStakedBcoins, uint64_t &globalOwedScoins) const;
    uint64_t GetGlobalCollateralRatio(const uint64_t bcoinMedianPrice) const;

    bool CheckGlobalCollateralRatioFloorReached(const uint64_t bcoinMedianPrice,
                                                const uint64_t globalCollateralRatioLimit);
    bool CheckGlobalCollateralCeilingReached(const uint64_t newBcoinsToStake, const uint64_t globalCollateralCeiling);

    void SetBaseViewPtr(CCdpDBCache *pBaseIn);
    void SetDbOpLogMap(CDBOpLogMap * pDbOpLogMapIn);

    void RegisterUndoFunc(UndoDataFuncMap &undoDataFuncMap) {
        globalStakedBcoinsCache.RegisterUndoFunc(undoDataFuncMap);
        globalOwedScoinsCache.RegisterUndoFunc(undoDataFuncMap);
        cdpCache.RegisterUndoFunc(undoDataFuncMap);
        regId2CDPCache.RegisterUndoFunc(undoDataFuncMap);
        ratioCDPIdCache.RegisterUndoFunc(undoDataFuncMap);
    }

    uint32_t GetCacheSize() const;
    bool Flush();

private:
    bool SaveCDPToDB(const CUserCDP &cdp);
    bool EraseCDPFromDB(const CUserCDP &cdp);

    // Usage: before modification, erase the old cdp; after modification, save the new cdp.
    bool SaveCDPToRatioDB(const CUserCDP &userCdp);
    bool EraseCDPFromRatioDB(const CUserCDP &userCdp);

private:
    /*  CSimpleKVCache          prefixType                     value               variable           */
    /*  -------------------- --------------------           -------------       --------------------- */
    CSimpleKVCache<         dbk::CDP_GLOBAL_STAKED_BCOINS,   uint64_t>      globalStakedBcoinsCache;
    CSimpleKVCache<         dbk::CDP_GLOBAL_OWED_SCOINS,     uint64_t>      globalOwedScoinsCache;

    /*  CCompositeKVCache     prefixType     key                            value             variable  */
    /*  ----------------   --------------   ------------                --------------    ----- --------*/
    // cdp{$cdpid} -> CUserCDP
    CCompositeKVCache<      dbk::CDP,       uint256,                    CUserCDP>           cdpCache;
    // rcdp${CRegID} -> set<cdpid>
    CCompositeKVCache<      dbk::REGID_CDP, CRegIDKey,                     set<uint256>>       regId2CDPCache;
    // cdpr{Ratio}{$cdpid} -> CUserCDP
    RatioCDPIdCache           ratioCDPIdCache;
};

enum CDPCloseType: uint8_t {
    BY_REDEEM = 0,
    BY_MANUAL_LIQUIDATE,
    BY_FORCE_LIQUIDATE
};

string GetCdpCloseTypeName(const CDPCloseType type);

class CClosedCdpDBCache {
public:
    CClosedCdpDBCache() {}

    CClosedCdpDBCache(CDBAccess *pDbAccess) : closedCdpTxCache(pDbAccess), closedTxCdpCache(pDbAccess) {}

    CClosedCdpDBCache(CClosedCdpDBCache *pBaseIn)
        : closedCdpTxCache(&pBaseIn->closedCdpTxCache), closedTxCdpCache(&pBaseIn->closedTxCdpCache) {}

public:
    bool AddClosedCdpIndex(const uint256& closedCdpId, const uint256& closedCdpTxId, CDPCloseType closeType) {
        return closedCdpTxCache.SetData(closedCdpId, {closedCdpTxId, (uint8_t)closeType});
    }

    bool AddClosedCdpTxIndex(const uint256& closedCdpTxId, const uint256& closedCdpId, CDPCloseType closeType) {
        return  closedTxCdpCache.SetData(closedCdpTxId, {closedCdpId, closeType});
    }

    bool GetClosedCdpById(const uint256& closedCdpId, std::pair<uint256, uint8_t>& cdp) {
        return closedCdpTxCache.GetData(closedCdpId, cdp);
    }

    bool GetClosedCdpByTxId(const uint256& closedCdpTxId, std::pair<uint256, uint8_t>& cdp) {
        return closedTxCdpCache.GetData(closedCdpTxId, cdp);
    }

    uint32_t GetCacheSize() const { return closedCdpTxCache.GetCacheSize() + closedTxCdpCache.GetCacheSize(); }

    void SetBaseViewPtr(CClosedCdpDBCache *pBaseIn) {
        closedCdpTxCache.SetBase(&pBaseIn->closedCdpTxCache);
        closedTxCdpCache.SetBase(&pBaseIn->closedTxCdpCache);
    }

    void Flush() {
        closedCdpTxCache.Flush();
        closedTxCdpCache.Flush();
    }

    void SetDbOpLogMap(CDBOpLogMap *pDbOpLogMapIn) {
        closedCdpTxCache.SetDbOpLogMap(pDbOpLogMapIn);
        closedTxCdpCache.SetDbOpLogMap(pDbOpLogMapIn);
    }

    void RegisterUndoFunc(UndoDataFuncMap &undoDataFuncMap) {
        closedCdpTxCache.RegisterUndoFunc(undoDataFuncMap);
        closedTxCdpCache.RegisterUndoFunc(undoDataFuncMap);
    }
private:
    /*  CCompositeKVCache     prefixType     key               value             variable  */
    /*  ----------------   --------------   ------------   --------------    ----- --------*/
    // ccdp${closed_cdpid} -> <closedCdpTxId, closeType>
    CCompositeKVCache< dbk::CLOSED_CDP_TX, uint256, std::pair<uint256, uint8_t> > closedCdpTxCache;
    // ctx${$closed_cdp_txid} -> <closedCdpId, closeType> (no-force-liquidation)
    CCompositeKVCache< dbk::CLOSED_TX_CDP, uint256, std::pair<uint256, uint8_t> > closedTxCdpCache;
};

#endif  // PERSIST_CDPDB_H
