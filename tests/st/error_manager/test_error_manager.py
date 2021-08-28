# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
import os

import pytest


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_single
def test_hccl_init_fail():
    sh_path = os.path.split(os.path.realpath(__file__))[0]
    ret = os.system(f"sh {sh_path}/run_hccl_init_fail.sh")
    assert ret == 0
    grep_ret = os.system(f"grep 'Ascend error occurred, error message:' {sh_path}/test_hccl_init_fail.log -c")
    assert grep_ret == 0
    grep_ret = os.system(f"grep 'EI0004:' {sh_path}/test_hccl_init_fail.log -c")
    assert grep_ret == 0
    grep_ret = os.system(f"grep 'Invalid ranktable, with rankID' {sh_path}/test_hccl_init_fail.log -c")
    assert grep_ret == 0

@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_single
def test_tbe_compile_fail():
    sh_path = os.path.split(os.path.realpath(__file__))[0]
    ret = os.system(f"sh {sh_path}/run_tbe_compile_fail.sh")
    assert ret == 0
    grep_ret = os.system(f"grep 'Ascend error occurred, error message:' {sh_path}/test_tbe_compile_fail.log -c")
    assert grep_ret == 0
    grep_ret = os.system(f"grep 'E60011:' {sh_path}/test_tbe_compile_fail.log -c")
    assert grep_ret == 0
    grep_ret = os.system(r"grep 'In op\[conv2d\], the \[strideh\] must in range \[1, 63\], actual is \[99\]' "
                         + f"{sh_path}/test_tbe_compile_fail.log -c")
    assert grep_ret == 0
