#include <AbstractTableInterface.hpp>
#include <BlockchainInterface.hpp>

// We can't inline this because we need to see the real definition of BlockchainInterface to cast it to QObject
AbstractTableInterface::AbstractTableInterface(BlockchainInterface* blockchain, QString scope)
    : QObject(blockchain), blockchain(blockchain), scope(scope) {}
