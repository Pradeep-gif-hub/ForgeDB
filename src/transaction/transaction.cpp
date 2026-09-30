#include "forgedb/transaction/transaction.hpp"

namespace forgedb {

Transaction::Transaction(TransactionId txn_id, IsolationLevel level)
    : txn_id_(txn_id), isolation_level_(level) {}

}  // namespace forgedb
