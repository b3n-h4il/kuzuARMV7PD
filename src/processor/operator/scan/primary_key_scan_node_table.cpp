#include "processor/operator/scan/primary_key_scan_node_table.h"

#include "binder/expression/expression_util.h"
#include "processor/execution_context.h"

using namespace kuzu::common;
using namespace kuzu::storage;

namespace kuzu {
namespace processor {

std::string PrimaryKeyScanPrintInfo::toString() const {
    std::string result = "Key: ";
    result += key;
    if (!alias.empty()) {
        result += ",Alias: ";
        result += alias;
    }
    result += ", Expressions: ";
    result += binder::ExpressionUtil::toString(expressions);

    return result;
}

idx_t PrimaryKeyScanSharedState::getTableIdx() {
    std::unique_lock lck{mtx};
    if (cursor < numTables) {
        return cursor++;
    }
    return numTables;
}

void PrimaryKeyScanNodeTable::initLocalStateInternal(ResultSet* resultSet,
    ExecutionContext* context) {
    for (auto& nodeInfo : nodeInfos) {
        std::vector<const Column*> columns;
        columns.reserve(nodeInfo.columnIDs.size());
        for (const auto columnID : nodeInfo.columnIDs) {
            if (columnID == INVALID_COLUMN_ID) {
                columns.push_back(nullptr);
            } else {
                columns.push_back(&nodeInfo.table->getColumn(columnID));
            }
        }
        nodeInfo.localScanState = std::make_unique<NodeTableScanState>(nodeInfo.table->getTableID(),
            nodeInfo.columnIDs, columns);
        initVectors(*nodeInfo.localScanState, *resultSet);
    }
    indexEvaluator->init(*resultSet, context->clientContext);
}

void PrimaryKeyScanNodeTable::initVectors(TableScanState& state, const ResultSet& resultSet) const {
    ScanTable::initVectors(state, resultSet);
    state.rowIdxVector->state = state.nodeIDVector->state;
    state.outState = state.rowIdxVector->state.get();
}

bool PrimaryKeyScanNodeTable::getNextTuplesInternal(ExecutionContext* context) {
    auto transaction = context->clientContext->getTransaction();
    auto tableIdx = sharedState->getTableIdx();
    if (tableIdx >= nodeInfos.size()) {
        return false;
    }
    KU_ASSERT(tableIdx < nodeInfos.size());
    auto& nodeInfo = nodeInfos[tableIdx];

    indexEvaluator->evaluate();
    auto indexVector = indexEvaluator->resultVector.get();
    auto& selVector = indexVector->state->getSelVector();
    KU_ASSERT(selVector.getSelSize() == 1);
    auto pos = selVector.getSelectedPositions()[0];
    if (indexVector->isNull(pos)) {
        return false;
    }

    offset_t nodeOffset = 0;
    const bool lookupSucceed = nodeInfo.table->lookupPK(transaction, indexVector, pos, nodeOffset);
    if (!lookupSucceed) {
        return false;
    }
    auto nodeID = nodeID_t{nodeOffset, nodeInfo.table->getTableID()};
    nodeInfo.localScanState->nodeIDVector->setValue<nodeID_t>(pos, nodeID);
    nodeInfo.table->initScanState(transaction, *nodeInfo.localScanState, nodeID.tableID,
        nodeOffset);
    metrics->numOutputTuple.incrementByOne();
    return nodeInfo.table->lookup(transaction, *nodeInfo.localScanState);
}

} // namespace processor
} // namespace kuzu
