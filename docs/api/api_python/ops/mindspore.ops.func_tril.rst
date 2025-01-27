mindspore.ops.tril
===================

.. py:function:: mindspore.ops.tril(input_x, diagonal=0)

    返回单个矩阵（二维Tensor）或批次输入矩阵的下三角形部分，其他位置的元素将被置零。
    矩阵的下三角形部分定义为对角线本身和对角线以下的元素。

    参数：
        - **input_x** (Tensor) - 输入Tensor。shape为 :math:`(x_1, x_2, ..., x_R)` ，其rank至少为2。
          支持的数据类型有包括所有数值型和bool类型。
        - **diagonal** (int，可选) - 指定对角线位置，默认值：0，指定主对角线。

    返回：
        Tensor，其数据类型和shape维度与 `input_x` 相同。

    异常：
        - **TypeError** - 如果 `input_x` 不是Tensor。
        - **TypeError** - 如果 `diagonal` 不是int类型。
        - **TypeError** - 如果 `input_x` 的数据类型既不是数值型也不是bool。
        - **ValueError** - 如果 `input_x` 的秩小于2。
