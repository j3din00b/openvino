// Copyright (C) 2018-2025 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <node.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <oneapi/dnnl/dnnl.hpp>
#include <oneapi/dnnl/dnnl_common.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "cpu_memory.h"
#include "cpu_types.h"
#include "dnnl_postops_composer_legacy.h"
#include "graph_context.h"
#include "nodes/executors/eltwise.hpp"
#include "nodes/kernels/jit_eltwise_common.hpp"
#include "openvino/core/node.hpp"
#include "openvino/core/type.hpp"
#include "openvino/core/type/element_type.hpp"

namespace ov::intel_cpu::node {

enum class EltwiseImplType : uint8_t { reference = 0, optimized = 1, optimizedShapeAgnostic = 2 };
class Eltwise : public Node {
public:
    class IEltwiseExecutor {
    public:
        IEltwiseExecutor() = default;
        virtual void exec(const jit_eltwise_call_args_ptrs& args_ptrs, const VectorDims& dims_out) = 0;
        [[nodiscard]] virtual size_t getBatchDimIdx() const = 0;
        [[nodiscard]] virtual const VectorDims& getOutDims() const = 0;
        virtual ~IEltwiseExecutor() = default;
    };

    using executorPtr = std::shared_ptr<IEltwiseExecutor>;

    Eltwise(const std::shared_ptr<ov::Node>& op, const GraphContext::CPtr& context);

    void getSupportedDescriptors() override;
    void initSupportedPrimitiveDescriptors() override;
    void selectOptimalPrimitiveDescriptor() override;
    void execute(const dnnl::stream& strm) override;
    bool created() const override;
    bool canBeInPlace() const override;
    bool canFuseConvert(const NodePtr& convertNode);
    bool canFuseParent(const NodePtr& parentNode) const;
    bool canFuse(const NodePtr& node) const override;
    void appendPostOps(dnnl::post_ops& ops,
                       const VectorDims& postOpDims,
                       std::unordered_map<int, MemoryPtr>& postOpsMem,
                       int channelAxis) override;
    void appendPostOps(dnnl::post_ops& ops,
                       const VectorDims& postOpDims,
                       std::vector<const void*>& postOpsMem,
                       int channelAxis) override;
    bool appendAttrPostOps(DnnlPostOpsComposerLegacy& dnnlpoc,
                           bool isLastPostOp,
                           dnnl::memory::data_type outDataType,
                           bool allowBinary = true);
    void fuseInto(NodePtr& parentNode) override;
    ov::element::Type getRuntimePrecision() const override;

    float getAlpha() const {
        return alpha;
    }
    float getBeta() const {
        return beta;
    }
    float getGamma() const {
        return gamma;
    }
    const std::vector<float>& getScales() const {
        return scales;
    }
    const std::vector<float>& getShifts() const {
        return shifts;
    }

    dnnl::algorithm getOneDnnAlgorithm() const {
        return onednnAlgorithm;
    }

    bool isWithBroadcast();
    bool isSpecialConvolutionAddFusing() const {
        return specialConvolutionAddFusing;
    }

    bool needPrepareParams() const override;
    void prepareParams() override;
    void createPrimitive() override;

    void executeDynamicImpl(const dnnl::stream& strm) override;

    enum BroadcastingPolicy : uint8_t {
        PerChannel,
        PerTensor,
        Undefined,
    };

    BroadcastingPolicy getBroadcastingPolicy() const {
        return broadcastingPolicy;
    }

    static bool isSupportedOperation(const std::shared_ptr<const ov::Node>& op, std::string& errorMessage) noexcept;

private:
    executorPtr execPtr = nullptr;
    BroadcastingPolicy broadcastingPolicy = Undefined;

    dnnl::algorithm onednnAlgorithm = dnnl::algorithm::undef;

    EltwiseImplType implType = EltwiseImplType::reference;
    std::vector<bool> broadcastPolicy;
    bool specialConvolutionAddFusing = false;
    size_t inputNum = 0;
    std::vector<ptrdiff_t> start_offset_in;
    ptrdiff_t start_offset_out = 0;

    std::vector<ov::element::Type> inpPrc;
    ov::element::Type outPrc;

    // blocked dims for which kernel compiled and params prepared
    std::vector<VectorDims> currentInBlkDims;

    // shape agnostic kernel
    struct {
        VectorDims outDims;
        std::vector<VectorDims> inOffsets;
        VectorDims outOffsets;
    } execParams;

    float alpha = 0;
    float beta = 0;
    float gamma = 0;

    std::vector<float> scales;
    std::vector<float> shifts;
    MemoryPtr scalesMemory;
    MemoryPtr shiftsMemory;

    std::vector<float> depthwiseData;
    MemoryPtr depthwiseMemory;
    size_t depthwiseDataSize = 0;

    std::vector<MemoryPtr> memPtrs;
    std::vector<const void*> fqDataPtrs;

    using Initializer = std::function<void(const std::shared_ptr<ov::Node>&, Eltwise& node)>;
    static const std::map<const ov::DiscreteTypeInfo, Initializer>& getInitializers();

    static BroadcastingPolicy determineBroadcastingPolicy(const std::shared_ptr<ov::Node>& op);

    size_t getOpInputsNum() const;

    template <typename T>
    void appendPostOpsImpl(dnnl::post_ops& ops,
                           const VectorDims& postOpDims,
                           std::vector<T>& postOpsMem,
                           int channelAxis = 1);

    void appendMemory(const std::vector<float>& data, MemoryPtr& memPtr, std::vector<MemoryPtr>& postOpsMem);
    static void appendMemory(const std::vector<float>& data, MemoryPtr& memPtr, std::vector<const void*>& postOpsMem);

    bool canUseEltwiseExecPtr = false;
    EltwiseAttrs eltwiseAttrs;
    std::shared_ptr<EltwiseExecutor> eltwiseExecPtr = nullptr;
};

}  // namespace ov::intel_cpu::node
