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

#ifndef MINDSPORE_CORE_OPS_MAX_POOL_GRAD_GRAD_H_
#define MINDSPORE_CORE_OPS_MAX_POOL_GRAD_GRAD_H_
#include <map>
#include <vector>
#include <string>
#include <memory>
#include "ops/base_operator.h"
#include "mindapi/base/types.h"

namespace mindspore {
namespace ops {
constexpr auto kNameMaxPoolGradGrad = "MaxPoolGradGrad";
class MIND_API MaxPoolGradGrad : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(MaxPoolGradGrad);
  MaxPoolGradGrad() : BaseOperator(kNameMaxPoolGradGrad) {
    InitIOName({"orig_input", "orig_output", "grad"}, {"output"});
  }
  void set_kernel_size(const std::vector<int64_t> &kernel_size);
  std::vector<int64_t> get_kernel_size() const;

  void set_pad_mode(const PadMode &pad_mode);
  PadMode get_pad_mode() const;

  void set_strides(const std::vector<int64_t> &strides);
  std::vector<int64_t> get_strides() const;
};

abstract::AbstractBasePtr MaxPoolGradGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                               const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_MAX_POOL_GRAD_GRAD_H_