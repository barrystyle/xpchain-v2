// Copyright (c) 2019-2020 barrystyle/xpchain team
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <chainparams.h>
#include <script/standard.h>
#include <util.h>
#include <validation.h>

double_t GetAnnualRateLegacy(int nHeight);
CAmount GetProofOfStakeRewardLegacy(int nHeight, CAmount nAmount, uint32_t nTime);
uint256 GetRewardHash(const std::vector<std::pair<CScript, CAmount>>& vReward, CTransactionRef txCoinStake, uint32_t nTime);
bool IsCoinStakeTx(CTransactionRef tx, const Consensus::Params &consensusParams, uint256 &hashBlock, CTransactionRef& prevTx);
bool IsDestinationSame(const CScript& a, const CScript& b);
bool EqualDestination(CTransactionRef txCoinStake, CPubKey pubkey);
bool VerifyCoinBaseTx(const CBlock& block, CValidationState& state);
bool knownAbuseOfInputBug(const uint256 checkHash);
