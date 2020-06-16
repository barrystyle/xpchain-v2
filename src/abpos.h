// Copyright (c) 2019-2020 barrystyle/xpchain team
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <chainparams.h>
#include <util.h>
#include <validation.h>

bool isMainnet();
int GetLastHeight(uint256 txHash);
void GetPoSPhase(int nHeight, int& nPhase, int& nBlocks);
int GetPoSInterest(int nHeight, int nCoinAge);
CAmount GetStakeReward(CAmount coinAmount, int nCoinAge);

