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

#define USE_DEPRECATED_API
#include "tools/converter/quantizer/bias_correction_strategy.h"
#include <dirent.h>
#include <future>
#include <set>
#include <memory>
#include <functional>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include "src/common/log_adapter.h"
#include "include/errorcode.h"
#include "mindapi/base/type_id.h"
#include "tools/common/tensor_util.h"

namespace mindspore::lite::quant {
namespace {
constexpr int kHasBiasTensorSize = 4;
const std::set<std::string> kSupportBiasCorrectionNode = {
  schema::EnumNamePrimitiveType(schema::PrimitiveType_Conv2DFusion)};
}  // namespace

int BiasCorrectionStrategy::QuantOriginFeatureMap(const float *origin_feature_map_data,
                                                  size_t origin_feature_map_data_size,
                                                  const std::vector<lite::LiteQuantParam> &feature_map_quant_params,
                                                  size_t quant_size, std::vector<int8_t> *quant_datas) {
  CHECK_NULL_RETURN(quant_datas);
  if (feature_map_quant_params.size() != 1) {
    MS_LOG(ERROR) << "unexpected feature_map_quant_params size: " << feature_map_quant_params.size();
    return RET_ERROR;
  }
  schema::QuantParamT feature_map_quant_param_t;
  feature_map_quant_param_t.scale = feature_map_quant_params[0].scale;
  feature_map_quant_param_t.zeroPoint = feature_map_quant_params[0].zeroPoint;
  for (size_t i = 0; i < origin_feature_map_data_size; i++) {
    auto quant_data = QuantizeData<int8_t>(origin_feature_map_data[i], &feature_map_quant_param_t, activation_q_max_,
                                           activation_q_min_);
    quant_datas->push_back(quant_data);
  }
  if (quant_size != quant_datas->size() * sizeof(int8_t)) {
    MS_LOG(ERROR) << "unexpected feature_map_tensor size: " << quant_datas->size()
                  << " not the same with: " << quant_datas->size() * sizeof(int8_t);
    return RET_ERROR;
  }
  return RET_OK;
}

void BiasCorrectionStrategy::CalcAccumulativeError(const CallBackParam &call_param,
                                                   const std::vector<float> &fp32_op_output_ch_mean,
                                                   const std::vector<float> &dequant_op_output_ch_mean) {
  std::vector<float> bias_diff(fp32_op_output_ch_mean.size());
  // Calculate current layer diff
  std::transform(fp32_op_output_ch_mean.begin(), fp32_op_output_ch_mean.end(), dequant_op_output_ch_mean.begin(),
                 bias_diff.begin(), std::minus<>());

  // Accumulate the diff of all rounds
  if (op_bias_diff_sum_map_.find(call_param.node_name) != op_bias_diff_sum_map_.end()) {
    auto &accumulate_bias_diff = op_bias_diff_sum_map_[call_param.node_name];
    std::transform(accumulate_bias_diff.begin(), accumulate_bias_diff.end(), bias_diff.begin(),
                   accumulate_bias_diff.begin(), std::plus<>());
  } else {
    op_bias_diff_sum_map_[call_param.node_name] = bias_diff;
  }
}

bool BiasCorrectionStrategy::OpInputDataHandle(OperationType type, const string &op_name, std::vector<float> *data) {
  MS_ASSERT(data != nullptr);
  std::lock_guard<std::mutex> lg(mutex_op_input_);
  if (type == STORE) {
    if (fp32_op_input_map_.find(op_name) != fp32_op_input_map_.end()) {
      // the data has not been fetched by int8 model
      return false;
    }
    fp32_op_input_map_[op_name] = *data;
    return true;
  } else if (type == FETCH) {
    if (fp32_op_input_map_.find(op_name) == fp32_op_input_map_.end()) {
      // the data not generated by fp32 model yet
      return false;
    }
    *data = fp32_op_input_map_[op_name];
    fp32_op_input_map_.erase(op_name);
    return true;
  } else {
    MS_LOG(ERROR) << "unexpected type: " << type;
  }
  return false;
}

bool BiasCorrectionStrategy::OpOutputChMeanDataHandle(OperationType type, const string &op_name,
                                                      std::vector<float> *data) {
  MS_ASSERT(data != nullptr);
  std::lock_guard<std::mutex> lg(mutex_op_output_);
  if (type == STORE) {
    if (fp32_op_output_ch_mean_map_.find(op_name) != fp32_op_output_ch_mean_map_.end()) {
      // the data has not been fetched by int8 model
      return false;
    }
    fp32_op_output_ch_mean_map_[op_name] = *data;
    return true;
  } else if (type == FETCH) {
    if (fp32_op_output_ch_mean_map_.find(op_name) == fp32_op_output_ch_mean_map_.end()) {
      // the data not generated by fp32 model yet
      return false;
    }
    *data = fp32_op_output_ch_mean_map_[op_name];
    fp32_op_output_ch_mean_map_.erase(op_name);
    return true;
  } else {
    MS_LOG(ERROR) << "unexpected type: " << type;
  }
  return false;
}

KernelCallBack BiasCorrectionStrategy::GetBeforeCallBack(CallBackType call_back_flag) {
  switch (call_back_flag) {
    case CPUFP32:
      return GetCPUFloatBeforeCallBack();
    case CPUInt8:
      return GetCPUInt8BeforeCallBack();
    case NVGPUInt8:
      return GetNVGPUInt8BeforeCallBack();
    default:
      MS_LOG(ERROR) << "Unsupported call back flag:" << call_back_flag;
      return nullptr;
  }
}

KernelCallBack BiasCorrectionStrategy::GetAfterCallBack(CallBackType call_back_flag) {
  switch (call_back_flag) {
    case CPUFP32:
      return GetCPUFloatAfterCallBack();
    case CPUInt8:
      return GetCPUInt8AfterCallBack();
    case NVGPUInt8:
      return GetNVGPUInt8AfterCallBack();
    default:
      MS_LOG(ERROR) << "Unsupported call back flag:" << call_back_flag;
      return nullptr;
  }
}

KernelCallBack BiasCorrectionStrategy::GetCPUFloatBeforeCallBack() {
  auto before_call_back = [this](const std::vector<mindspore::tensor::MSTensor *> &before_inputs,
                                 const std::vector<mindspore::tensor::MSTensor *> &before_outputs,
                                 const CallBackParam &call_param) -> bool {
    auto is_skip_op = quant_strategy_->IsSkipOp(call_param.node_name);
    if (kSupportBiasCorrectionNode.find(call_param.node_type) == kSupportBiasCorrectionNode.end() || is_skip_op) {
      return true;
    }
    auto tensor = before_inputs[0];
    MS_ASSERT(tensor != nullptr);
    // op can be skipped.
    if (tensor->data_type() != kNumberTypeFloat32) {
      MS_LOG(INFO) << "tensor type is " << tensor->data_type();
      return true;
    }
    size_t elem_count = tensor->ElementsNum();
    MS_CHECK_GT(elem_count, 0, false);
    std::vector<float> fp32_op_input(elem_count);
    auto ret = memcpy_s(fp32_op_input.data(), fp32_op_input.size() * sizeof(float), tensor->data(), tensor->Size());
    if (ret != EOK) {
      MS_LOG(ERROR) << "memcpy error: " << ret;
      return false;
    }
    while (!OpInputDataHandle(STORE, call_param.node_name, &fp32_op_input)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kMillisecondsBase));
    }
    return true;
  };
  return before_call_back;
}

KernelCallBack BiasCorrectionStrategy::GetCPUInt8BeforeCallBack() {
  auto before_call_back = [this](const std::vector<mindspore::tensor::MSTensor *> &before_inputs,
                                 const std::vector<mindspore::tensor::MSTensor *> &before_outputs,
                                 const CallBackParam &call_param) -> bool {
    auto is_skip_op = quant_strategy_->IsSkipOp(call_param.node_name);
    if (kSupportBiasCorrectionNode.find(call_param.node_type) == kSupportBiasCorrectionNode.end() || is_skip_op) {
      return true;
    }
    auto tensor = before_inputs[0];
    MS_ASSERT(tensor != nullptr);
    // op can be skipped.
    if (tensor->data_type() != kNumberTypeInt8) {
      MS_LOG(INFO) << "tensor type is " << tensor->data_type();
      return true;
    }
    // Get origin data
    std::vector<float> fp32_op_input;
    while (!OpInputDataHandle(FETCH, call_param.node_name, &fp32_op_input)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kMillisecondsBase));
    }
    std::vector<int8_t> quant_datas;
    // do quantization: activation is always per layer quantized
    auto ret = QuantOriginFeatureMap(fp32_op_input.data(), fp32_op_input.size(), tensor->quant_params(), tensor->Size(),
                                     &quant_datas);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "Quant origin feature map failed.";
      return ret;
    }
    ret = memcpy_s(tensor->data(), tensor->Size(), quant_datas.data(), quant_datas.size() * sizeof(int8_t));
    if (ret != EOK) {
      MS_LOG(ERROR) << "memcpy error: " << ret;
      return false;
    }
    return true;
  };

  return before_call_back;
}

KernelCallBack BiasCorrectionStrategy::GetCPUInt8AfterCallBack() {
  auto after_call_back = [this](const std::vector<mindspore::tensor::MSTensor *> &after_inputs,
                                const std::vector<mindspore::tensor::MSTensor *> &after_outputs,
                                const CallBackParam &call_param) -> bool {
    auto is_skip_op = quant_strategy_->IsSkipOp(call_param.node_name);
    if (kSupportBiasCorrectionNode.find(call_param.node_type) == kSupportBiasCorrectionNode.end() || is_skip_op) {
      return true;
    }
    auto tensor = after_outputs[0];
    MS_ASSERT(tensor != nullptr);
    // op can be skipped.
    if (tensor->data_type() != kNumberTypeInt8) {
      MS_LOG(INFO) << "tensor type is " << tensor->data_type();
      return true;
    }
    std::vector<float> fp32_op_output_ch_mean;
    while (!OpOutputChMeanDataHandle(FETCH, call_param.node_name, &fp32_op_output_ch_mean)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kMillisecondsBase));
    }

    // Calculate the difference between original and quantified
    // DeQuant Data
    std::vector<double> dequant_data;
    auto ret = DeQuantData(tensor, &dequant_data);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "DeQuant data failed.";
      return false;
    }
    std::vector<float> dequant_op_output_ch_mean;
    // Calculate output per channel means
    ret = CalculatePerChannelMeans<double>(dequant_data.data(), dequant_data.size(), tensor->shape(),
                                           &dequant_op_output_ch_mean);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "Calculate Per channel means failed.";
      return false;
    }

    CalcAccumulativeError(call_param, fp32_op_output_ch_mean, dequant_op_output_ch_mean);
    return true;
  };
  return after_call_back;
}

KernelCallBack BiasCorrectionStrategy::GetCPUFloatAfterCallBack() {
  auto after_call_back = [this](const std::vector<mindspore::tensor::MSTensor *> &after_inputs,
                                const std::vector<mindspore::tensor::MSTensor *> &after_outputs,
                                const CallBackParam &call_param) -> bool {
    auto is_skip_op = quant_strategy_->IsSkipOp(call_param.node_name);
    if (kSupportBiasCorrectionNode.find(call_param.node_type) == kSupportBiasCorrectionNode.end() || is_skip_op) {
      return true;
    }
    auto tensor = after_outputs[0];
    MS_ASSERT(tensor != nullptr);
    // op can be skipped.
    if (tensor->data_type() != kNumberTypeFloat32) {
      MS_LOG(INFO) << "tensor type is " << tensor->data_type();
      return true;
    }
    std::vector<float> fp32_op_output_ch_mean;
    // Calculate output per channel means
    auto ret = CalculatePerChannelMeans<float>(static_cast<float *>(tensor->data()), tensor->ElementsNum(),
                                               tensor->shape(), &fp32_op_output_ch_mean);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "Calculate Per channel means failed.";
      return false;
    }
    while (!OpOutputChMeanDataHandle(STORE, call_param.node_name, &fp32_op_output_ch_mean)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kMillisecondsBase));
    }
    return true;
  };
  return after_call_back;
}

int BiasCorrectionStrategy::Int8Inference(const KernelCallBack &before_call_back,
                                          const KernelCallBack &after_call_back) {
  // int8 inference
  std::vector<mindspore::tensor::MSTensor *> inputs = int8_session_->GetInputs();
  for (size_t i = 0; i < calibrator_->GetBatchNum(); i++) {
    for (size_t input_index = 0; input_index < inputs.size(); input_index++) {
      int status = calibrator_->GenerateInputData(inputs[input_index]->tensor_name(), i, inputs[input_index]);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "generate input data failed!";
        return RET_ERROR;
      }
    }
    int8_session_->BindThread(true);
    auto status = int8_session_->RunGraph(before_call_back, after_call_back);
    int8_session_->BindThread(false);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "run model failed!";
      return RET_ERROR;
    }
  }  // end for images
  return RET_OK;
}

int BiasCorrectionStrategy::Fp32Inference(const KernelCallBack &before_call_back,
                                          const KernelCallBack &after_call_back) {
  // get input tensor
  std::vector<mindspore::tensor::MSTensor *> inputs = fp32_session_->GetInputs();
  // fp32 inference
  for (size_t i = 0; i < calibrator_->GetBatchNum(); i++) {
    for (size_t input_index = 0; input_index < inputs.size(); input_index++) {
      int status = calibrator_->GenerateInputData(inputs[input_index]->tensor_name(), i, inputs[input_index]);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "generate input data from images failed!";
        return RET_ERROR;
      }
    }
    fp32_session_->BindThread(true);
    auto status = fp32_session_->RunGraph(before_call_back, after_call_back);
    fp32_session_->BindThread(false);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "run model failed!";
      return RET_ERROR;
    }
  }  // end for images
  return RET_OK;
}

int BiasCorrectionStrategy::CreateQuantModel(const FuncGraphPtr &quant_func_graph) {
  // init in8 session
  MS_LOG(INFO) << "create quant session";
  flags_.commonQuantParam.quant_type = schema::QuantType_QUANT_ALL;
  auto int8_sm = CreateSessionByFuncGraph(quant_func_graph, flags_, this->flags_.commonQuantParam.thread_num);
  int8_session_ = int8_sm.session;
  int8_model_ = int8_sm.model;
  CHECK_NULL_RETURN(int8_session_);
  CHECK_NULL_RETURN(int8_model_);
  return RET_OK;
}

int BiasCorrectionStrategy::DoCPUBiasCorrection(const FuncGraphPtr &quant_func_graph) {
  CHECK_NULL_RETURN(calibrator_);
  CHECK_NULL_RETURN(quant_strategy_);
  CHECK_NULL_RETURN(fp32_session_);
  CHECK_NULL_RETURN(fp32_model_);
  int8_before_call_back_ = GetBeforeCallBack(CPUInt8);
  int8_after_call_back_ = GetAfterCallBack(CPUInt8);
  fp32_before_call_back_ = GetBeforeCallBack(CPUFP32);
  fp32_after_call_back_ = GetAfterCallBack(CPUFP32);
  return DoBiasCorrection(quant_func_graph, true);
}

int BiasCorrectionStrategy::DoNVGPUBiasCorrection(const FuncGraphPtr &quant_func_graph) {
  CHECK_NULL_RETURN(calibrator_);
  CHECK_NULL_RETURN(quant_strategy_);
  CHECK_NULL_RETURN(fp32_session_);
  CHECK_NULL_RETURN(fp32_model_);
  int8_before_call_back_ = GetBeforeCallBack(NVGPUInt8);
  int8_after_call_back_ = GetAfterCallBack(NVGPUInt8);
  fp32_before_call_back_ = GetBeforeCallBack(CPUFP32);
  fp32_after_call_back_ = GetAfterCallBack(CPUFP32);
  return DoBiasCorrection(quant_func_graph, false);
}

int BiasCorrectionStrategy::DoBiasCorrection(const FuncGraphPtr &quant_func_graph, bool int32_bias) {
  auto ret = CreateQuantModel(quant_func_graph);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Create quant model failed:" << ret;
    return ret;
  }
  if (calibrator_->GetBatchNum() == 0) {
    MS_LOG(ERROR) << "divisor 'calibrate_size' cannot be 0.";
    return RET_ERROR;
  }

  std::future<int> int8_inference = std::async(std::launch::async, &BiasCorrectionStrategy::Int8Inference, this,
                                               int8_before_call_back_, int8_after_call_back_);
  auto status = Fp32Inference(fp32_before_call_back_, fp32_after_call_back_);
  if (status != RET_OK) {
    MS_LOG(ERROR) << "int8 inference failed!";
    return RET_ERROR;
  }
  status = int8_inference.get();
  if (status != RET_OK) {
    MS_LOG(ERROR) << "int8 inference failed!";
    return RET_ERROR;
  }
  // Calculate the mean of the error.
  for (auto &key_value : op_bias_diff_sum_map_) {
    std::for_each(key_value.second.begin(), key_value.second.end(),
                  [this](float &data) { data = data / calibrator_->GetBatchNum(); });
  }
  auto cnodes = quant_func_graph->GetOrderedCnodes();
  for (auto &cnode : cnodes) {
    auto op_name = cnode->fullname_with_scope();
    if (op_bias_diff_sum_map_.find(op_name) == op_bias_diff_sum_map_.end()) {
      continue;
    }
    status = DoCNodeBiasCorrection(quant_func_graph, cnode, int32_bias);
    if (status != RET_OK) {
      MS_LOG(ERROR) << op_name << " do node bias correct failed.";
      break;
    }
  }
  return status;
}

int BiasCorrectionStrategy::CreateFp32BiasTensor(const FuncGraphPtr &quant_func_graph, const CNodePtr &cnode,
                                                 const ParameterPtr &parameter, const std::vector<float> &bias_diff) {
  auto op_name = cnode->fullname_with_scope();
  if (parameter == nullptr) {
    MS_LOG(ERROR) << op_name << " parameter is nullptr.";
    return RET_NULL_PTR;
  }
  std::vector<int64_t> shape;
  shape.push_back(bias_diff.size());

  auto tensor_info = CreateTensorInfo(bias_diff.data(), sizeof(float) * bias_diff.size(), shape, kNumberTypeFloat32);
  if (tensor_info == nullptr) {
    MS_LOG(ERROR) << op_name << " create tensor info failed.";
    return RET_ERROR;
  }
  auto status = InitParameterFromTensorInfo(parameter, tensor_info);
  if (status != RET_OK) {
    MS_LOG(ERROR) << op_name << " init parameter from tensor info failed";
    return RET_ERROR;
  }
  parameter->set_name("added_" + op_name + "_bias");
  cnode->add_input(parameter);
  return RET_OK;
}

int BiasCorrectionStrategy::AddBiasToInt32Tensor(const CNodePtr &cnode, const tensor::TensorPtr &bias_tensor,
                                                 const std::vector<schema::QuantParamT> &bias_quant_params,
                                                 const std::vector<float> &bias_diff) {
  auto op_name = cnode->fullname_with_scope();
  int *bias_datas = static_cast<int *>(bias_tensor->data_c());
  if (static_cast<size_t>(bias_tensor->DataSize()) != bias_diff.size()) {
    MS_LOG(ERROR) << op_name << " unexpected bias data count: " << bias_tensor->DataSize()
                  << " not the same as bias_diff: " << bias_diff.size();
    return RET_ERROR;
  }
  if (bias_quant_params.size() != bias_diff.size()) {
    MS_LOG(ERROR) << op_name << " unexpected bias quant params size: " << bias_quant_params.size()
                  << " not the same as bias_diff: " << bias_diff.size();
    return RET_ERROR;
  }
  for (size_t i = 0; i < bias_tensor->DataSize(); i++) {
    auto scale = bias_quant_params[i].scale;
    if (fabs(scale) <= 0.0f) {
      MS_LOG(ERROR) << op_name << " divisor 'scale' cannot be 0.";
      return RET_ERROR;
    }
    double after_correct = std::round(bias_diff[i] / scale) + bias_datas[i];
    const constexpr int32_t corrected_bias_abs_limit = 0.6 * INT32_MAX;
    if (after_correct > corrected_bias_abs_limit) {
      MS_LOG(WARNING) << op_name << " ch: " << i << " bias after_corrected too large: " << after_correct
                      << " origin value: " << bias_datas[i] << " bias_diff: " << bias_diff[i] << " scale: " << scale;
      bias_datas[i] = static_cast<int>(corrected_bias_abs_limit);
    } else if (after_correct < -corrected_bias_abs_limit) {
      MS_LOG(WARNING) << op_name << " ch: " << i << " bias after_corrected too small: " << after_correct
                      << " origin value: " << bias_datas[i] << " bias_diff: " << bias_diff[i] << " scale: " << scale;
      bias_datas[i] = static_cast<int>(-corrected_bias_abs_limit);
    } else {
      auto diff = static_cast<int>(std::round(bias_diff[i] / scale));
      bias_datas[i] += diff;
    }
  }
  return RET_OK;
}

int BiasCorrectionStrategy::AddBiasToFp32Tensor(const CNodePtr &cnode, const tensor::TensorPtr &bias_tensor,
                                                const std::vector<float> &bias_diff) {
  auto op_name = cnode->fullname_with_scope();
  auto bias_datas = static_cast<float *>(bias_tensor->data_c());
  if (static_cast<size_t>(bias_tensor->DataSize()) != bias_diff.size()) {
    MS_LOG(ERROR) << op_name << " unexpected bias data count: " << bias_tensor->DataSize()
                  << " not the same as bias_diff: " << bias_diff.size();
    return RET_ERROR;
  }
  if (bias_tensor->DataSize() != bias_diff.size()) {
    MS_LOG(ERROR) << op_name << " unexpected bias size: " << bias_tensor->DataSize()
                  << " not the same as bias_diff: " << bias_diff.size();
    return RET_ERROR;
  }
  for (size_t i = 0; i < bias_tensor->DataSize(); i++) {
    bias_datas[i] += bias_diff[i];
  }
  return RET_OK;
}

int BiasCorrectionStrategy::DoCNodeBiasCorrection(const FuncGraphPtr &quant_func_graph, const CNodePtr &cnode,
                                                  bool int32_bias) {
  auto op_name = cnode->fullname_with_scope();
  const auto &bias_diff = op_bias_diff_sum_map_[op_name];
  auto primitive = GetValueNode<PrimitivePtr>(cnode->input(0));
  if (primitive == nullptr) {
    MS_LOG(ERROR) << op_name << " primitive is nullptr.";
    return RET_NULL_PTR;
  }
  auto quant_param_holder = GetCNodeQuantHolder(primitive);
  MS_CHECK_TRUE_MSG(quant_param_holder != nullptr, RET_NULL_PTR, "quant_param_holder is nullptr.");
  auto input_quant_params = quant_param_holder->get_input_quant_params();
  if (cnode->size() == kHasBiasTensorSize) {
    auto bias = cnode->input(THIRD_INPUT + 1);
    auto bias_parameter_ptr = bias->cast<ParameterPtr>();
    auto bias_default_param = bias_parameter_ptr->default_param();
    auto bias_tensor = bias_default_param->cast<tensor::TensorPtr>();
    if (int32_bias) {
      auto bias_quant_params = input_quant_params.at(THIRD_INPUT);
      auto status = AddBiasToInt32Tensor(cnode, bias_tensor, bias_quant_params, bias_diff);
      if (status != RET_OK) {
        MS_LOG(ERROR) << op_name << " Add bias to int32 tensor failed.";
        return RET_ERROR;
      }
    } else {
      auto status = AddBiasToFp32Tensor(cnode, bias_tensor, bias_diff);
      if (status != RET_OK) {
        MS_LOG(ERROR) << op_name << " Add bias to int32 tensor failed.";
        return RET_ERROR;
      }
    }
  } else if (cnode->size() == kHasBiasTensorSize - 1) {
    MS_LOG(INFO) << op_name << " add bias input";
    // need to add bias input
    auto parameter = quant_func_graph->add_parameter();
    auto status = CreateFp32BiasTensor(quant_func_graph, cnode, parameter, bias_diff);
    if (status != RET_OK) {
      MS_LOG(ERROR) << op_name << " Create fp32 bias tensor failed.";
      return RET_ERROR;
    }
    if (int32_bias) {
      status = DoParameterBiasQuant(parameter, primitive);
      if (status != RET_OK) {
        MS_LOG(ERROR) << op_name << " Do bias quant failed.";
        return RET_ERROR;
      }
    }
  } else {
    MS_LOG(WARNING) << op_name << " unexpected size: " << cnode->size();
  }
  return RET_OK;
}

KernelCallBack BiasCorrectionStrategy::GetNVGPUInt8BeforeCallBack() {
  auto before_call_back = [this](const std::vector<mindspore::tensor::MSTensor *> &before_inputs,
                                 const std::vector<mindspore::tensor::MSTensor *> &before_outputs,
                                 const CallBackParam &call_param) -> bool {
    auto is_skip_op = quant_strategy_->IsSkipOp(call_param.node_name);
    if (kSupportBiasCorrectionNode.find(call_param.node_type) == kSupportBiasCorrectionNode.end() || is_skip_op) {
      return true;
    }
    auto feature_map_tensor = before_inputs[0];
    MS_ASSERT(feature_map_tensor != nullptr);
    // op can be skipped.
    if (feature_map_tensor->data_type() != kNumberTypeFloat32) {
      MS_LOG(INFO) << "feature_map_tensor type is " << feature_map_tensor->data_type();
      return true;
    }
    // Get origin data
    std::vector<float> fp32_op_input;
    while (!OpInputDataHandle(FETCH, call_param.node_name, &fp32_op_input)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kMillisecondsBase));
    }
    // do quantization: activation is always per layer quantized
    std::vector<int8_t> quant_datas;
    auto ret = QuantOriginFeatureMap(static_cast<float *>(feature_map_tensor->data()),
                                     feature_map_tensor->ElementsNum(), feature_map_tensor->quant_params(),
                                     feature_map_tensor->ElementsNum() * sizeof(int8_t), &quant_datas);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << feature_map_tensor->tensor_name() << " Quant origin feature map failed. " << ret;
      return false;
    }
    std::vector<float> dequant_data;
    ret = DeQuantData(quant_datas.data(), quant_datas.size(), feature_map_tensor->quant_params(), &dequant_data);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "DeQuant origin feature map failed. " << ret;
      return false;
    }
    ret = memcpy_s(feature_map_tensor->data(), feature_map_tensor->Size(), dequant_data.data(),
                   dequant_data.size() * sizeof(float));
    if (ret != EOK) {
      MS_LOG(ERROR) << "memcpy error: " << ret;
      return false;
    }
    return true;
  };
  return before_call_back;
}

KernelCallBack BiasCorrectionStrategy::GetNVGPUInt8AfterCallBack() {
  auto after_call_back = [this](const std::vector<mindspore::tensor::MSTensor *> &after_inputs,
                                const std::vector<mindspore::tensor::MSTensor *> &after_outputs,
                                const CallBackParam &call_param) -> bool {
    auto is_skip_op = quant_strategy_->IsSkipOp(call_param.node_name);
    if (kSupportBiasCorrectionNode.find(call_param.node_type) == kSupportBiasCorrectionNode.end() || is_skip_op) {
      return true;
    }
    auto tensor = after_outputs[0];
    MS_ASSERT(tensor != nullptr);
    // op can be skipped.
    if (tensor->data_type() != kNumberTypeFloat32) {
      MS_LOG(INFO) << "tensor type is " << tensor->data_type();
      return true;
    }
    std::vector<float> fp32_op_output_ch_mean;
    while (!OpOutputChMeanDataHandle(FETCH, call_param.node_name, &fp32_op_output_ch_mean)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kMillisecondsBase));
    }

    // Calculate the difference between original and quantified
    // Calculate output per channel means
    std::vector<float> dequant_op_output_ch_mean;
    auto ret = CalculatePerChannelMeans<float>(static_cast<float *>(tensor->data()), tensor->ElementsNum(),
                                               tensor->shape(), &dequant_op_output_ch_mean);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "Calculate Per channel means failed.";
      return false;
    }
    CalcAccumulativeError(call_param, fp32_op_output_ch_mean, dequant_op_output_ch_mean);
    return true;
  };
  return after_call_back;
}
}  // namespace mindspore::lite::quant
