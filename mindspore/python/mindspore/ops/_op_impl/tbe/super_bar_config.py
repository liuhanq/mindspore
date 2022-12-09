# Copyright 2020 Huawei Technologies Co., Ltd
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

"""Abs op"""
from mindspore.ops.op_info_register import load_super_bar_config

super_bar_config = {
    "NodeAttrMap": {
        "AvgPool3DD": {
            "ksize": "kernel_size",
            "pads": "pad_list",
            "data_format": "format"
        },
        "AvgPool3DGradD": {
            "orig_input_shape": "origin_input_shape",
            "ksize": "kernel_size",
            "pads": "pad_list",
            "data_format": "format"
        },
        "AvgPoolGradD": {
            "orig_input_shape": "x_origin",
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "data_format": "format"
        },
        "AccumulateNV2": {
            "N": "n"
        },
        "AddN": {
            "N": "n"
        },
        "Conv2D": {
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "Conv3DTransposeD": {
            "pads": "pad_list",
            "data_format": "format"
        },
        "Conv3D": {
            "pads": "pad_list",
            "data_format": "format"
        },
        "Conv3DBackpropInputD": {
            "pads": "pad_list",
            "data_format": "format"
        },
        "Conv3DBackpropFilterD": {
            "pads": "pad_list",
            "data_format": "format"
        },
        "Conv2DTransposeD": {
            "input_size": "input_sizes",
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "MaxPoolWithArgmax": {
            "ksize": "kernel_size",
            "padding": "pad_mode"
        },
        "SoftmaxGradExt": {
            "axes": "axis",
            "keep_dims": "keepdims"
        },
        "Conv2DBackpropFilterD": {
            "filter_size": "filter_sizes",
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "Conv2DBackpropInputD": {
            "input_size": "input_sizes",
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "Conv2DBackpropFilter": {
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "Conv2DBackpropInput": {
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "ConcatD": {
            "concat_dim": "axis"
        },
        "DepthwiseConv2D": {
            "strides": "stride",
            "dilations": "dilation",
            "pads": "pad_list",
            "data_format": "format",
            "offfset_x": "offset_a"
        },
        "ExtractVolumePatches": {
            "ksizes": "kernel_size"
        },
        "L2Normalize": {
            "eps": "epsilon"
        },
        "L2NormalizeGrad": {
            "dim": "axis",
            "eps": "epsilon"
        },
        "MaxPoolGradWithArgmax": {
            "ksize": "kernel_size",
            "padding": "pad_mode"
        },
        "BiasAddGrad": {
            "data_format": "format"
        },
        "FusedDbnDw": {
            "filter_size": "filter_sizes",
            "strides": "stride",
            "pads": "pad_list",
            "dilations": "dilation",
            "data_format": "format"
        },
        "MaxPool": {
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "data_format": "format"
        },
        "ReduceMeanD": {
            "axes": "axis"
        },
        "ReduceSumD": {
            "axes": "axis"
        },
        "ReduceAnyD": {
            "axes": "axis"
        },
        "ReduceMaxD": {
            "axes": "axis"
        },
        "ReduceMinD": {
            "axes": "axis"
        },
        "ReduceAllD": {
            "axes": "axis"
        },
        "ReduceProdD": {
            "axes": "axis"
        },
        "ReduceStd": {
            "dim": "axis",
            "keepdim": "keep_dims"
        },
        "SoftmaxV2": {
            "axes": "axis"
        },
        "LogSoftmaxV2": {
            "axes": "axis"
        },
        "ArgMaxD": {
            "dimension": "axis"
        },
        "BatchMatMul": {
            "adj_x1": "transpose_x1",
            "adj_x2": "transpose_x2"
        },
        "BatchMatMulV2": {
            "adj_x1": "transpose_x1",
            "adj_x2": "transpose_x2"
        },
        "BatchNormal": {
            "data_format": "format"
        },
        "ArgMaxWithValue": {
            "dimension": "axis"
        },
        "SplitD": {
            "split_dim": "axis",
            "num_split": "output_num"
        },
        "BiasAdd": {
            "data_format": "format"
        },
        "SliceD": {
            "offsets": "begin"
        },
        "MaxPoolGrad": {
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "data_format": "format"
        },
        "MaxPool3D": {
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "pads": "pad_list",
            "data_format": "format"
        },
        "MaxPool3DGrad": {
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "pads": "pad_list",
            "data_format": "format"
        },
        "MaxPoolGradGrad": {
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "data_format": "format"
        },
        "AvgPool": {
            "ksize": "kernel_size",
            "padding": "pad_mode",
            "data_format": "format"
        },
        "BatchNorm": {
            "data_format": "format"
        },
        "BatchNormGrad": {
            "data_format": "format"
        },
        "ArgMin": {
            "dimension": "axis"
        },
        "ArgMinD": {
            "dimension": "axis"
        },
        "LpNorm": {
            "axes": "axis",
            "keepdim": "keep_dims"
        },
        "SmoothL1LossV2": {
            "sigma": "beta"
        },
        "Roll": {
            "shifts": "shift",
            "dims": "axis"
        },
        "SmoothL1LossGradV2": {
            "sigma": "beta"
        },
        "Centralization": {
            "axes": "axis"
        },
        "MaxPool3DGradGradD": {
            "ksize": "kernel_size"
        }
    },
    "AttrDefaultValue": {
        "Log": {
            "base": "-1.0",
            "scale": "1.0",
            "shift": "0.0"
        },
        "ScatterNdUpdate": {
            "use_locking": "false"
        },
        "OneHotD": {
            "axis": "-1"
        },
        "Iou": {
            "mode": "iou",
            "eps": "1.0"
        },
        "GatherV2D": {
            "axis": "-1"
        },
        "MaxPoolGrad": {
            "data_format": "NHWC"
        },
        "ResizeNearestNeighborV2D": {
            "align_corners": "false",
            "half_pixel_centers": "false"
        },
        "ResizeNearestNeighborV2GradD": {
            "align_corners": "false",
            "half_pixel_centers": "false"
        },
        "MaxPool3D": {
            "pads": "0,0,0",
            "dilation": "1,1,1",
            "ceil_mode": "0"
        }
    },
    "InputOrders": {
        "LogSoftmaxGrad": [1, 0],
        "LayerNormGrad": [1, 0, 2, 3, 4],
        "LayerNormBetaGammaBackprop": [1, 0, 2, 3],
        "LayerNormXBackprop": [1, 0, 2, 3, 4],
        "LayerNormXBackpropV2": [1, 0, 2, 3, 4],
        "ApplyCenteredRMSPropD": [0, 1, 2, 3, 5, 6, 7, 8, 4],
        "Conv2DBackpropInputD": [1, 0],
        "Conv2DBackpropFilterD": [1, 0],
        "MinimumGrad": [2, 0, 1],
        "MaximumGrad": [2, 0, 1],
        "StridedSliceGrad": [1, 2, 3, 4, 0],
        "Conv2DBackpropInput": [2, 1, 0],
        "Conv2DBackpropFilter": [1, 2, 0],
        "FusionOp_Conv2DBackpropInputD_AddN_ReluGradV2": [1, 0, 2, 3],
        "FusionOp_Conv2DBackpropInputD_ReluGradV2": [1, 0, 2]
    },
    "SkipDynamicCompileStatic": ["SoftmaxV2", "PRelu", "Trunc", "AccumulateNV2",
                                 "SoftmaxCrossEntropyWithLogits", "ReduceMeanD", "SquareSumV1", "BiasAddGrad"],
    # BroadcastTo: The name is occupied
    # DynamicBroadcastTo: The name is occupied
    # BatchToSpaceD: attr type is listInt，not listListInt
    # BatchToSpaceNDD: attr type is listInt，not listListInt
    # SpaceToBatchD: attr type is listInt，not listListInt
    # SpaceToBatchNDD: attr type is listInt，not listListInt
    # DynamicGRUV2: input4 is None, GE will change to hidden op by pass
    # DynamicRNN: input4 is None, GE will change to hidden op by pass
    # KLDivLossGrad: Accuracy issues
    # ScatterNdUpdate: not support int8 in op json
    # ScatterNdAdd:  not support int8 in op json
    # UnsortedSegmentSum: check support failed when shape is -2
    # ConcatOffset: Hadn't adapted tbe implementation
    # MirrorPad: Hadn't adapted tbe implementation
    # InplaceIndexAdd: check support failed if var has only one dimension
    # Expand: Hadn't adapted tbe implementation
    # ExpandD: Hadn't adapted tbe implementation
    # Cross: Hadn't adapted tbe implementation
    # LinSpaceD: Hadn't adapted tbe implementation
    # Cast: Accuracy issues
    # AvgPool3DGradD second device format is facz_3d, but in json, the key is ndhwc
    # DataFormatDimMap:  attr order swap
    # DepthwiseConv2D: Accuracy issues(second format is error in python)
    # ACos: dynamic impl errpr
    # TransData: support boll
    # ScatterNdD: Accuracy issues
    # Trace: Hadn't adapted tbe implementation
    "SkipNodes": ["BroadcastTo", "DynamicBroadcastTo", "BatchToSpaceD", "BatchToSpaceNDD", "SpaceToBatchD",
                  "SpaceToBatchNDD", "DynamicGRUV2", "DynamicRNN", "KLDivLossGrad", "ScatterNdUpdate", "ScatterNdAdd",
                  "ConcatOffset", "MirrorPad", "InplaceIndexAdd", "Expand", "ExpandD", "Cross", "LinSpaceD", "Cast",
                  "AvgPool3DGradD", "DataFormatDimMap", "DepthwiseConv2D", "Trace", "ACos", "TransData", "ScatterNdD"]
}


@load_super_bar_config(super_bar_config)
def _call_super_bar_config():
    """super bar config register"""
    return
