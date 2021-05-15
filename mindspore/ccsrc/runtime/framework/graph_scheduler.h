/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_SCHEDULER_H_
#define MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_SCHEDULER_H_

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <fstream>
#include "runtime/framework/actor/data_source_actor.h"
#include "runtime/framework/actor/loop_count_actor.h"
#include "runtime/framework/actor/kernel_actor.h"
#include "runtime/framework/actor/output_actor.h"
#include "runtime/framework/actor/switch_actor.h"
#include "runtime/hardware/device_context.h"
#include "backend/session/kernel_graph.h"

namespace mindspore {
namespace runtime {
using mindspore::device::DeviceContext;
using mindspore::session::KernelWithIndex;
using KernelMapActor = std::unordered_map<std::string, KernelActorPtr>;
using KernelMapPosition = std::map<KernelWithIndex, size_t, session::KernelWithIndexCmp>;
using ActorInfo = std::string;

enum class GraphExecutionStrategy {
  kPipeline,  // The actor running is triggered only by data.
  kStep       // The actor running need be triggered by control in addition.
};

// The graph compiler info generated by graph compiler is the express of executable graph.
// The device context is unified interface of interaction with device of corresponding graph.
// The input tensor is used to link graphs in the dynamic build scenario.
// The control node is used to link graphs in the control flow scenario.
// The origin parameters order is used to correspond to the input args.
// The origin outputs order is used to correspond to the output args.
struct GraphCompilerInfo {
  GraphCompilerInfo(const std::vector<KernelGraphPtr> &graphs, const std::vector<DeviceContext *> &device_contexts,
                    const std::vector<std::vector<TensorPtr> *> &input_tensors,
                    const std::vector<AnfNodePtr> &control_nodes,
                    const std::vector<AnfNodePtr> &origin_parameters_order,
                    const KernelMapPosition &origin_outputs_order, const std::string &name)
      : graphs_(graphs),
        device_contexts_(device_contexts),
        input_tensors_(input_tensors),
        control_nodes_(control_nodes),
        origin_parameters_order_(origin_parameters_order),
        origin_outputs_order_(origin_outputs_order),
        name_(name) {}
  std::vector<KernelGraphPtr> graphs_;
  std::vector<DeviceContext *> device_contexts_;
  std::vector<std::vector<TensorPtr> *> input_tensors_;
  std::vector<AnfNodePtr> control_nodes_;
  std::vector<AnfNodePtr> origin_parameters_order_;
  KernelMapPosition origin_outputs_order_;
  std::string name_;
};

// The actor set generated by graph transformer is the execution unit of actor runtime.
// It includes data source actor, kernel actor, loop count actor.
// The data source actor is used to obtain data and process them into device tensors,
// and then send them to kernel actor. The kernel actor is used to receive the device tensors to luanch kernel.
// Specifically notice the no input kernel actor, it means that this actor has no input device tensor, need be triggered
// externally. The loop count actor is used to receive the control of tail kernel actor to represent the end of one step
// and decide whether to loop execution by loop count. The output actor is used to receive the output result of actor
// which represents the graph output.
struct ActorSet {
  explicit ActorSet(const ActorInfo &name) : name_(name) {}
  std::vector<DataSourceActorPtr> data_source_actors_;
  std::vector<KernelActorPtr> kernel_actors_;
  // No input kernel actors need be triggered specifically.
  std::vector<KernelActorPtr> no_input_kernel_actors_;
  std::vector<SwitchActorPtr> switch_actors_;
  LoopCountActorPtr loop_count_actor_{nullptr};
  OutputActorPtr output_actor_{nullptr};
  ActorInfo name_;
};
using ActorSetPtr = std::shared_ptr<ActorSet>;

class GraphScheduler {
 public:
  static GraphScheduler &GetInstance() {
    static GraphScheduler instance;
    return instance;
  }

  // 1. Thread pool creating.
  // 2. The memory manager creating and scheduling.
  void Initialize();

  // Transform graph to actor DAG, contains build and link.
  ActorSet *Transform(const GraphCompilerInfo &graph_compiler_info,
                      GraphExecutionStrategy strategy = GraphExecutionStrategy::kPipeline);

  // Schedule actors in the actor runtime. Single machine scheduling is supported currently, and distributed scheduling
  // will be supported in the future.
  void Schedule(const ActorSet *actor_set);

  // The prepare processing before run:
  // 1. Prepare the data of device tensor store(such as weights and value nodes of graph).
  // 2. Prepare the data of host tensor queue(such as non weighted parameters of graph).
  // 3. Prepare the continuous memory for communication kernel.
  void PrepareRun(const ActorSet *actor_set, const GraphCompilerInfo &graph_compiler_info,
                  const std::vector<std::vector<TensorPtr>> &input_tensors);

  // The processing entry of actors running.
  bool Run(const ActorSet *actor_set, GraphExecutionStrategy strategy = GraphExecutionStrategy::kPipeline);

  // Fetch the actor set by actor info.
  ActorSet *Fetch(const ActorInfo &actor_info) const;

 private:
  GraphScheduler() = default;
  ~GraphScheduler() = default;
  DISABLE_COPY_AND_ASSIGN(GraphScheduler);

  // Transform the nodes of graph to actors.
  ActorSetPtr Build(const GraphCompilerInfo &graph_compiler_info);
  // Link actors to DAG through the edge connection of graph and graph execution strategy.
  void Link(ActorSet *actor_set, const GraphCompilerInfo &graph_compiler_info, GraphExecutionStrategy strategy);

  // The processing of actors build.
  std::vector<DataSourceActorPtr> BuildDataSourceActor(const GraphCompilerInfo &graph_compiler_info,
                                                       const HostTensorQueuePtr &host_queue);
  std::vector<KernelActorPtr> BuildKernelActor(const GraphCompilerInfo &graph_compiler_info);
  LoopCountActorPtr BuildLoopCountActor(const GraphCompilerInfo &graph_compiler_info);
  OutputActorPtr BuildOutputActor(const GraphCompilerInfo &graph_compiler_info);
  std::vector<KernelActorPtr> BuildNoInputKernelActor(const ActorSet *actor_set);

  // The processing of actors link.
  void LinkDataArrowForDeviceDSActor(DeviceQueueDataSourceActor *from_actor, KernelActor *to_actor,
                                     KernelWithIndex from_kernel_with_output_idx,
                                     KernelWithIndex to_to_kernel_with_input_idx);
  void LinkDataArrowForHostDSActor(HostQueueDataSourceActor *from_actor, KernelActor *to_actor,
                                   KernelWithIndex from_kernel_with_output_idx,
                                   KernelWithIndex to_kernel_with_input_idx);
  void LinkDataArrowForKernelActor(KernelActor *from_actor, KernelActor *to_actor,
                                   KernelWithIndex from_kernel_with_output_idx,
                                   KernelWithIndex to_kernel_with_input_idx);
  void LinkControlArrowForKernelActor(std::vector<KernelActorPtr> *from_actors, LoopCountActor *to_actor,
                                      GraphExecutionStrategy strategy);
  void LinkControlArrowForLoopCountActor(LoopCountActor *loop_count_actor, const ActorSet *actor_set);
  void LinkControlArrowByAutoMonad(KernelActor *to_actor, const AnfNodePtr &from_node,
                                   const KernelMapActor &kernel_actors_map);
  void LinkOutputResultArrowForOutputActor(OutputActor *to_actor,
                                           const std::vector<DataSourceActorPtr> &data_source_actors,
                                           const KernelMapActor &kernel_actors_map,
                                           const GraphCompilerInfo &graph_compiler_info);

  // Check whether the actor set is valid.
  bool CheckActorValid(const ActorSet *actor_set) const;

  // Persist device tensors of graph's some nodes(such as weights and value nodes).
  void PersistDeviceTensor(const GraphCompilerInfo &graph_compiler_info);

  // Fetch the hsot tensor queue by actor info.
  HostTensorQueue *FetchHostQueue(const ActorInfo &actor_info) const;

  // Display the actor information of corresponding kernel graph.
  void DumpActor(const ActorSet *actor_set) const;
  void DumpDSActor(const DataSourceActor *actor, std::ofstream &ofs) const;
  void DumpLoopCountActor(const LoopCountActor *actor, std::ofstream &ofs) const;
  void DumpKernelActor(const KernelActor *actor, std::ofstream &ofs) const;
  void DumpOutputActor(const OutputActor *actor, std::ofstream &ofs) const;

  std::unordered_map<ActorInfo, ActorSetPtr> actors_;
  std::unordered_map<ActorInfo, HostTensorQueuePtr> actor_to_host_queue_;

  // The second element of pair represents the output index of kernel actor corresponding to the device tensor.
  std::unordered_map<DeviceTensorPtr, std::pair<KernelActorPtr, int>> device_address_to_actor_;

  // The id of memory manager actor.
  AID memory_manager_aid_;

  bool init_{false};
};
}  // namespace runtime
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_SCHEDULER_H_
