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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_ORMQR_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_ORMQR_GPU_KERNEL_H_

#include <cublas_v2.h>
#include <cuda_runtime_api.h>
#include <cusolverDn.h>
#include <cuda_runtime.h>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <map>
#include <utility>
#include "mindspore/core/ops/ormqr.h"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/complex.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/transpose_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"

namespace mindspore {
namespace kernel {
template <typename T>
using Complex = mindspore::utils::Complex<T>;
class OrmqrGpuKernelMod : public NativeGpuKernelMod {
 public:
  OrmqrGpuKernelMod() { ResetResource(); }
  ~OrmqrGpuKernelMod() = default;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    if (is_null_input_) {
      return true;
    }
    cuda_stream_ = stream_ptr;
    return launch_kernel_func_(this, inputs, workspace, outputs);
  }
  bool Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
            const std::vector<KernelTensorPtr> &outputs) override;
  int Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
             const std::vector<KernelTensorPtr> &outputs, const std::map<uint32_t, tensor::TensorPtr> &) override;

  void ResetResource() noexcept {
    is_null_input_ = false;
    input_size_list_.clear();
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

  std::vector<KernelAttr> GetOpSupport() override;

 protected:
  template <typename T>
  void InitSizeLists();

  template <typename T>
  bool LaunchKernel(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                    const std::vector<AddressPtr> &outputs);
  bool CheckInputs();
  template <typename T>
  void RunOrmqr(T *d_a, T *tau, T *other, size_t lda, int *dev_info, T *d_output_y);
  template <typename T>
  void LaunchOrmqr(T *d_input_x, T *input_tau, T *d_input_other, T *d_output_y, int *dev_info);
  void CheckResult(int *dev_info);

  using LaunchKernelFunc =
    std::function<bool(OrmqrGpuKernelMod *, const std::vector<kernel::AddressPtr> &,
                       const std::vector<kernel::AddressPtr> &, const std::vector<kernel::AddressPtr> &)>;
  using InitSizeListsFunc = std::function<void(OrmqrGpuKernelMod *)>;
  LaunchKernelFunc launch_kernel_func_{nullptr};
  InitSizeListsFunc init_lists_func_{nullptr};
  static std::vector<std::pair<KernelAttr, std::pair<LaunchKernelFunc, InitSizeListsFunc>>> func_list_;

  bool left_{true};
  bool transpose_{false};
  std::vector<size_t> input_x_shape_;
  std::vector<size_t> input_tau_shape_;
  std::vector<size_t> input_other_shape_;
  size_t input_x_dims_{0};
  size_t input_tau_dims_{0};
  size_t input_other_dims_{0};
  size_t m_{0};
  size_t n_{0};
  size_t tau_n_{0};
  size_t x_m_{0};
  size_t x_n_{0};
  size_t batch_size_{0};
  bool is_null_input_;
  size_t transpose_input_x_shape_[TRANSPOSE_MAX_DIMENSION] = {0};
  size_t transpose_input_x_axis_[TRANSPOSE_MAX_DIMENSION] = {0};
  size_t transpose_input_other_shape_[TRANSPOSE_MAX_DIMENSION] = {0};
  size_t transpose_output_y_shape_[TRANSPOSE_MAX_DIMENSION] = {0};
  cusolverDnHandle_t handle_{nullptr};
  cublasSideMode_t side_;
  cublasOperation_t trans_;
  void *cuda_stream_{nullptr};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_ORMQR_GPU_KERNEL_H_
