#pragma once

#include "columns.h"
#include "facades/blocks.h"
#include "facades/miners.h"
#include "facades/state.h"
#include "facades/storage.h"
#include "facades/transactions.h"
#include "facades/users.h"

namespace logpass {

struct DatabaseFacades {
    database::BlocksFacade blocks;
    database::MinersFacade miners;
    database::StateFacade state;
    database::StorageFacade storage;
    database::TransactionsFacade transactions;
    database::UsersFacade users;
};

}
