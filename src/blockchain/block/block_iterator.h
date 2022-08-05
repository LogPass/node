#pragma once

#include <blockchain/transactions/transaction.h>

namespace logpass {

class Block;

// used to iterate block transactions with correct order
class BlockIterator {
public:
    BlockIterator(const Block& block, size_t index);

    Transaction_cptr operator*() const;
    bool operator==(const BlockIterator& second) const;
    bool operator!=(const BlockIterator& second) const;
    BlockIterator& operator++();

private:
    const Block& m_block;
    size_t m_index;
};

}
