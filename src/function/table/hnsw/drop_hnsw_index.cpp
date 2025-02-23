#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "function/table/bind_data.h"
#include "function/table/hnsw/hnsw_index_functions.h"
#include "processor/execution_context.h"
#include "storage/index/hnsw_index_utils.h"
#include "storage/index/index_utils.h"

namespace kuzu {
namespace function {

struct DropHNSWIndexBindData final : TableFuncBindData {
    catalog::NodeTableCatalogEntry* tableEntry;
    std::string indexName;

    DropHNSWIndexBindData(catalog::NodeTableCatalogEntry* tableEntry, std::string indexName)
        : TableFuncBindData{0}, tableEntry{tableEntry}, indexName{std::move(indexName)} {}

    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<DropHNSWIndexBindData>(tableEntry, indexName);
    }
};

static std::unique_ptr<TableFuncBindData> bindFunc(main::ClientContext* context,
    const TableFuncBindInput* input) {
    storage::IndexUtils::validateAutoTransaction(*context, DropHNSWIndexFunction::name);
    const auto indexName = input->getLiteralVal<std::string>(0);
    const auto tableName = input->getLiteralVal<std::string>(1);
    const auto tableEntry = storage::IndexUtils::bindTable(*context, tableName, indexName,
        storage::IndexOperation::DROP);
    return std::make_unique<DropHNSWIndexBindData>(tableEntry, indexName);
}

static common::offset_t tableFunc(const TableFuncInput& input, TableFuncOutput&) {
    const auto& context = *input.context->clientContext;
    const auto sharedState = input.sharedState->ptrCast<TableFuncSharedState>();
    const auto morsel = sharedState->getMorsel();
    if (morsel.isInvalid()) {
        return 0;
    }
    const auto bindData = input.bindData->constPtrCast<DropHNSWIndexBindData>();
    context.getCatalog()->dropIndex(context.getTransaction(), bindData->tableEntry->getTableID(),
        bindData->indexName);
    return 1;
}

static std::string dropHNSWIndexTables(main::ClientContext& context,
    const TableFuncBindData& bindData) {
    const auto dropHNSWIndexBindData = bindData.constPtrCast<DropHNSWIndexBindData>();
    context.setToUseInternalCatalogEntry();
    std::string query = "";
    query += common::stringFormat("CALL _DROP_HNSW_INDEX('{}', '{}');",
        dropHNSWIndexBindData->indexName, dropHNSWIndexBindData->tableEntry->getName());
    query += common::stringFormat("DROP TABLE {};",
        storage::HNSWIndexUtils::getUpperGraphTableName(dropHNSWIndexBindData->indexName));
    query += common::stringFormat("DROP TABLE {};",
        storage::HNSWIndexUtils::getLowerGraphTableName(dropHNSWIndexBindData->indexName));
    return query;
}

function_set InternalDropHNSWIndexFunction::getFunctionSet() {
    function_set functionSet;
    std::vector inputTypes = {common::LogicalTypeID::STRING, common::LogicalTypeID::STRING};
    auto func = std::make_unique<TableFunction>(name, inputTypes);
    func->tableFunc = tableFunc;
    func->bindFunc = bindFunc;
    func->initSharedStateFunc = TableFunction::initSharedState;
    func->initLocalStateFunc = TableFunction::initEmptyLocalState;
    functionSet.push_back(std::move(func));
    return functionSet;
}

function_set DropHNSWIndexFunction::getFunctionSet() {
    function_set functionSet;
    std::vector inputTypes = {common::LogicalTypeID::STRING, common::LogicalTypeID::STRING};
    auto func = std::make_unique<TableFunction>(name, inputTypes);
    func->tableFunc = TableFunction::emptyTableFunc;
    func->bindFunc = bindFunc;
    func->initSharedStateFunc = TableFunction::initSharedState;
    func->initLocalStateFunc = TableFunction::initEmptyLocalState;
    func->rewriteFunc = dropHNSWIndexTables;
    functionSet.push_back(std::move(func));
    return functionSet;
}

} // namespace function
} // namespace kuzu
