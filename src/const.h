#pragma once

namespace logpass {

// version
const uint8_t kVersionMajor = 1;
const uint8_t kVersionMinor = 0;

// number of blocks kept in level0 of rocksdb with ability to remove them
const size_t kDatabaseRolbackableBlocks = 32;
const size_t kMinersQueueSize = 240;

// transactions
const size_t kTransactionMaxBlockIdDifference = 240; // 1 hour
const size_t kTransactionMaxSize = 65535;
const uint64_t kTransactionFee = 20'000'000; // 0.02 tokens
const uint64_t kStakingDuration = 360; // in days

// blocks
const size_t kBlockMaxTransactions = 32768;
const size_t kBlockMaxTransactionsSize = 32 * 1024 * 1024;
const size_t kBlockTransactionsPerChunk = 1024;
const uint32_t kBlockInterval = 15;
const uint32_t kBlocksPerDay = 86400 / kBlockInterval;

// user
const size_t kUserMaxKeys = 5;
const size_t kUserMaxSupervisors = 5;
const uint8_t kUserKeyMaxPower = 5;
const uint8_t kMaxPower = 100;
const size_t kUserPowerLevels = 5; // number of power levels (lowest, low, medium, high, highest)
const uint32_t kUserMinLockLength = 120; // 1 hour
const uint32_t kUserMaxLockLength = kBlocksPerDay * 30; // 30 days
const uint32_t kUserMaxUpdateDelay = kBlocksPerDay * 30; // 30 days
const uint64_t kUserDefaultBalance = 500;
const uint8_t kUserMaxFreeTransactions = 50;
const uint8_t kUserMinFreeTransactions = 4;

// initial number of tokens
const size_t kFirstUserBalance = 990'000'000'000'000'000;
const size_t kFirstUserStake = 10'000'000'000'000'000;
const size_t kTotalNumberOfTokens = kFirstUserBalance + kFirstUserStake;

// storage
const size_t kStoragePrefixMinLength = 4;
const size_t kStoragePrefixMaxLength = 16;
const size_t kStoragePrefixMaxAllowedUsers = 10;
const size_t kStorageEntryMaxValueLength = 65000;

// network
const uint8_t kNetworkProtocolVersion = 0x01;

const size_t kNetworkMaxPacketSize = 4 * 1024 * 1024; // 4 MB
const size_t kNetworkConnectionTimeout = 15; // in seconds
const size_t kNetworkMaxPendingConnections = 10;

const size_t kNetworkHighPriorityConnections = 10; // miners mining next blocks
const size_t kNetworkMediumPriorityOutgoingConnections = 5; // miners in top 500 miners, outgoing connections
const size_t kNetworkMediumPriorityIncomingConnections = 5; // miners in top 500 miners, incoming connections
const size_t kNetworkLowPriorityOutgoingConnections = 5; // other miners, outgoing connections
const size_t kNetworkLowPriorityIncomingConnections = 5; // other miners, incoming connections

}
