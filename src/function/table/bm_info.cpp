#include "binder/binder.h"
#include "function/table/bind_data.h"
#include "function/table/table_function.h"
#include "main/database.h"
#include "storage/buffer_manager/buffer_manager.h"
#include "storage/buffer_manager/memory_manager.h"

namespace kuzu {
namespace function {

struct BMInfoBindData final : TableFuncBindData {
    uint64_t memLimit;
    uint64_t memUsage;

    BMInfoBindData(uint64_t memLimit, uint64_t memUsage, binder::expression_vector columns)
        : TableFuncBindData{std::move(columns), 1}, memLimit{memLimit}, memUsage{memUsage} {}

    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<BMInfoBindData>(memLimit, memUsage, columns);
    }
};

static common::offset_t tableFunc(const TableFuncInput& input, TableFuncOutput& output) {
    KU_ASSERT(output.dataChunk.getNumValueVectors() == 2);
    const auto sharedState = input.sharedState->ptrCast<TableFuncSharedState>();
    const auto morsel = sharedState->getMorsel();
    if (!morsel.hasMoreToOutput()) {
        return 0;
    }
    const auto bindData = input.bindData->constPtrCast<BMInfoBindData>();
    output.dataChunk.getValueVectorMutable(0).setValue<uint64_t>(0, bindData->memLimit);
    output.dataChunk.getValueVectorMutable(1).setValue<uint64_t>(0, bindData->memUsage);
    return 1;
}

static std::unique_ptr<TableFuncBindData> bindFunc(const main::ClientContext* context,
    const TableFuncBindInput* input) {
    auto memLimit = context->getMemoryManager()->getBufferManager()->getMemoryLimit();
    auto memUsage = context->getMemoryManager()->getBufferManager()->getUsedMemory();
    std::vector<common::LogicalType> returnTypes;
    returnTypes.emplace_back(common::LogicalType::UINT64());
    returnTypes.emplace_back(common::LogicalType::UINT64());
    auto returnColumnNames = std::vector<std::string>{"mem_limit", "mem_usage"};
    returnColumnNames =
        TableFunction::extractYieldVariables(returnColumnNames, input->yieldVariables);
    auto columns = input->binder->createVariables(returnColumnNames, returnTypes);
    return std::make_unique<BMInfoBindData>(memLimit, memUsage, columns);
}

function_set BMInfoFunction::getFunctionSet() {
    function_set functionSet;
    auto function = std::make_unique<TableFunction>(name, std::vector<common::LogicalTypeID>{});
    function->tableFunc = tableFunc;
    function->bindFunc = bindFunc;
    function->initSharedStateFunc = TableFunction::initSharedState;
    function->initLocalStateFunc = TableFunction::initEmptyLocalState;
    functionSet.push_back(std::move(function));
    return functionSet;
}

} // namespace function
} // namespace kuzu
