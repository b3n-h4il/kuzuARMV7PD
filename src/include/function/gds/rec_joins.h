#pragma once

#include "common/enums/extend_direction.h"
#include "common/enums/path_semantic.h"
#include "function/gds/gds.h"
#include "function/gds/gds_frontier.h"
#include "function/gds/gds_state.h"
#include "output_writer.h"

namespace kuzu {
namespace function {

struct RJBindData : public GDSBindData {
    static constexpr uint16_t DEFAULT_MAXIMUM_ALLOWED_UPPER_BOUND = (uint16_t)255;

    std::shared_ptr<binder::Expression> nodeInput = nullptr;
    // For any form of shortest path lower bound must always be 1.
    // If lowerBound equals to 0, an empty path with source node only will be returned.
    uint16_t lowerBound = 0;
    uint16_t upperBound = 0;
    common::PathSemantic semantic = common::PathSemantic::WALK;

    common::ExtendDirection extendDirection = common::ExtendDirection::FWD;

    bool flipPath = false; // See PathsOutputWriterInfo::flipPath for comments.
    bool writePath = true;

    std::shared_ptr<binder::Expression> directionExpr = nullptr;
    std::shared_ptr<binder::Expression> lengthExpr = nullptr;
    std::shared_ptr<binder::Expression> pathNodeIDsExpr = nullptr;
    std::shared_ptr<binder::Expression> pathEdgeIDsExpr = nullptr;

    std::string weightPropertyName;
    std::shared_ptr<binder::Expression> weightOutputExpr = nullptr;

    RJBindData(graph::GraphEntry graphEntry, std::shared_ptr<binder::Expression> nodeOutput)
        : GDSBindData{std::move(graphEntry), std::move(nodeOutput)} {}
    RJBindData(const RJBindData& other);

    bool hasNodeInput() const override { return true; }
    std::shared_ptr<binder::Expression> getNodeInput() const override { return nodeInput; }
    bool hasNodeOutput() const override { return true; }

    PathsOutputWriterInfo getPathWriterInfo() const;

    std::unique_ptr<GDSBindData> copy() const override {
        return std::make_unique<RJBindData>(*this);
    }
};

struct RJCompState {
    std::unique_ptr<GDSComputeState> gdsComputeState;
    std::unique_ptr<RJOutputWriter> outputWriter;

    RJCompState(std::unique_ptr<GDSComputeState> gdsComputeState,
        std::unique_ptr<RJOutputWriter> writer)
        : gdsComputeState{std::move(gdsComputeState)}, outputWriter{std::move(writer)} {}
};

class RJAlgorithm : public GDSAlgorithm {
    static constexpr char DIRECTION_COLUMN_NAME[] = "direction";
    static constexpr char LENGTH_COLUMN_NAME[] = "length";
    static constexpr char PATH_NODE_IDS_COLUMN_NAME[] = "pathNodeIDs";
    static constexpr char PATH_EDGE_IDS_COLUMN_NAME[] = "pathEdgeIDs";

public:
    RJAlgorithm() = default;
    RJAlgorithm(const RJAlgorithm& other) : GDSAlgorithm{other} {}

    void exec(processor::ExecutionContext* context) override;

    virtual RJCompState getRJCompState(processor::ExecutionContext* context,
        common::nodeID_t sourceNodeID) = 0;

    // Set algorithm to NOT track path
    void setToNoPath();
    binder::expression_vector getResultColumnsNoPath();

protected:
    void validateLowerUpperBound(int64_t lowerBound, int64_t upperBound);

    binder::expression_vector getBaseResultColumns() const;
    void bindColumnExpressions(binder::Binder* binder) const;

    std::unique_ptr<BFSGraph> getBFSGraph(processor::ExecutionContext* context);
};

class SPAlgorithm : public RJAlgorithm {
public:
    SPAlgorithm() = default;
    SPAlgorithm(const SPAlgorithm& other) : RJAlgorithm{other} {}

    // Inputs are graph, srcNode, upperBound, direction
    std::vector<common::LogicalTypeID> getParameterTypeIDs() const override {
        return {common::LogicalTypeID::ANY, common::LogicalTypeID::NODE,
            common::LogicalTypeID::INT64, common::LogicalTypeID::STRING};
    }

    void bind(const GDSBindInput& input, main::ClientContext&) override;
};

} // namespace function
} // namespace kuzu
