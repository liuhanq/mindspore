/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include <functional>
#include "proto/topology.pb.h"
#include "distributed/cluster/topology/utils.h"
#include "distributed/cluster/topology/meta_server_node.h"

namespace mindspore {
namespace distributed {
namespace cluster {
namespace topology {
bool MetaServerNode::Initialize() {
  // Init the address of meta server node.
  RETURN_IF_FALSE_WITH_LOG(FillMetaServerAddress(&meta_server_addr_),
                           "Failed to init the address of meta server node.");

  // Init the TCP server.
  RETURN_IF_FALSE_WITH_LOG(InitTCPServer(), "Failed to create the TCP server.");

  start_time_ = Now();

  // Init the thread for monitoring the state of the cluster topo.
  topo_monitor_ = std::thread(&MetaServerNode::UpdateTopoState, this);
  return true;
}

bool MetaServerNode::Finalize() {
  // Release the TCP server.
  if (tcp_server_ != nullptr) {
    tcp_server_->Finalize();
    tcp_server_.reset();
  }

  // Stop the topo monitor thread.
  enable_monitor_ = false;
  topo_monitor_.join();
  return true;
}

bool MetaServerNode::InitTCPServer() {
  tcp_server_ = std::make_unique<rpc::TCPServer>();
  MS_EXCEPTION_IF_NULL(tcp_server_);
  RETURN_IF_FALSE_WITH_LOG(tcp_server_->Initialize(meta_server_addr_.GetUrl()), "Failed to init the tcp server.");
  tcp_server_->SetMessageHandler(std::bind(&MetaServerNode::HandleMessage, this, std::placeholders::_1));

  // Configure the message processors for the TCP server.
  message_handlers_[MessageName::kRegistration] =
    std::bind(&MetaServerNode::ProcessRegister, this, std::placeholders::_1);
  message_handlers_[MessageName::kHeartbeat] =
    std::bind(&MetaServerNode::ProcessHeartbeat, this, std::placeholders::_1);
  return true;
}

void MetaServerNode::HandleMessage(const std::shared_ptr<MessageBase> &message) {
  MS_EXCEPTION_IF_NULL(message);
  const auto &message_name = static_cast<MessageName>(std::stoi(message->Name()));
  const auto &handler = message_handlers_.find(message_name);
  if (handler == message_handlers_.end()) {
    MS_LOG(ERROR) << "Unknown message name: " << message->Name();
  }
  message_handlers_[message_name](message);
}

void MetaServerNode::ProcessRegister(const std::shared_ptr<MessageBase> &message) {
  MS_EXCEPTION_IF_NULL(message);

  RegistrationMessage registration;
  const std::string &body = message->Body();
  registration.ParseFromArray(body.c_str(), body.length());

  // Add the compute graph node into registered nodes.
  const auto &node_id = registration.node_id();
  std::unique_lock<std::shared_mutex> lock(nodes_mutex_);
  if (nodes_.find(node_id) == nodes_.end()) {
    std::shared_ptr<ComputeGraphNodeState> node_state = std::make_shared<ComputeGraphNodeState>(node_id);
    nodes_[node_id] = node_state;
    MS_LOG(INFO) << "The new node: " << node_id << " is registered successfully.";
  } else {
    MS_LOG(ERROR) << "The node: " << node_id << " have been registered before.";
  }
}

void MetaServerNode::ProcessHeartbeat(const std::shared_ptr<MessageBase> &message) {
  MS_EXCEPTION_IF_NULL(message);

  HeartbeatMessage heartbeat;
  const std::string &body = message->Body();
  heartbeat.ParseFromArray(body.c_str(), body.length());

  // Update the state(timestamp) of this node.
  const auto &node_id = heartbeat.node_id();
  std::shared_lock<std::shared_mutex> lock(nodes_mutex_);
  if (nodes_.find(node_id) != nodes_.end()) {
    auto &node = nodes_[node_id];
    time(&(node->last_update));
  } else {
    MS_LOG(ERROR) << "Invalid node: " << node_id << ".";
  }
}

void MetaServerNode::UpdateTopoState() {
  while (enable_monitor_) {
    if (topo_state_ == TopoState::kInitializing) {
      // Set the state of topo to `kFailed` if the topology is still in process of initializtion but timed out.
      if (ElapsedTime(start_time_) > kTopoInitTimeout) {
        MS_LOG(ERROR) << "Failed to initialize the cluster topology after waiting for " << kTopoInitTimeout.count()
                      << " milliseconds.";
        topo_state_ = TopoState::kFailed;
      }

      std::shared_lock<std::shared_mutex> lock(nodes_mutex_);
      if (nodes_.size() == total_node_num_) {
        MS_LOG(INFO) << "The cluster topology has been constructed successfully";
        topo_state_ = TopoState::kInitialized;
        continue;
      }
      MS_LOG(INFO) << "The cluster topology is in the process of constructing, current alive node num: ("
                   << nodes_.size() << "/" << total_node_num_ << ")";
    }
    static const size_t interval = 3;
    sleep(interval);
  }
}

TopoState MetaServerNode::TopologyState() { return topo_state_; }

size_t MetaServerNode::GetAliveNodeNum() {
  std::shared_lock<std::shared_mutex> lock(nodes_mutex_);
  return nodes_.size();
}
}  // namespace topology
}  // namespace cluster
}  // namespace distributed
}  // namespace mindspore
