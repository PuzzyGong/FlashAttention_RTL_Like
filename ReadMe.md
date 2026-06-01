# FlashAttention RTL-Like 设计说明

## 为什么需要 C Model 和 UI

从验证/调试角度看，这类全流水结构比普通 module 难调：

1. 浮点数据不如整数直观，误差和特殊值都需要观察。
2. MAC、EXP、reciprocal 本身就是抽象算子，latency 和对齐关系容易出问题。
3. 多个全流水模块接在一起后，数据在不同 stage 之间的时间关系比较复杂。

所以 C model 和 UI 的目标不是替代 RTL，而是先把每拍数据、变量含义、clear/valid 对齐关系看清楚。

## 设计思路

常见做法一般有两类：

1. GPU / TPU / NPU 风格的 kernel 或 command 调度。优点是通用性强，但需要硬件调度器、软件编译/调度配合，软硬件耦合更复杂。
2. HLS / FPGA pipeline accelerator。开发快，但每个周期的数据位置和对齐关系不够直观，后期 debug 成本较高。

目前这个设计选择更直接的全流水 RTL 数据流：

```text
牺牲一部分可扩展性
换取更高资源利用率
换取更简单的硬件控制
换取更直接的验证路径
```

## 当前计算粒度

项目关注的是 LLM decode 阶段中，一个 transformer layer 内部的 attention 计算。粒度逐步缩小为：

```text
一个 batch
一个 decode step / token output
一个 transformer layer
一个 attention head
一个 attention tile
```

目前设定：

```text
head_d = 128
tile = 128
```

一个 tile 可以看成：

```text
Q: 128
K: 128 x 128
V: 128 x 128
```

如果输入 token 数是 4096，那么按 128 为 tile 粒度，需要计算 32 个 tile。

## 数据流结构

整体结构可以概括为：

```text
SRAM + handshake -> calculator pipeline -> SRAM
```

calculator pipeline 内部是全流水结构：

```text
QK -> S -> P -> PV -> O
```

这里的 `S`、`P`、`local_m`、`new_m`、`local_l`、`new_l`、`local_O`、`new_O` 不作为完整矩阵落 SRAM，而是在流水线中逐拍传递和更新。

粗略性能目标：

```text
模块内部 II = 1
tile-level II ~= 128 + 1 cycles
单 tile latency ~= 6 * 128 + 80 ~= 850 cycles
```

其中 latency 可以通过打拍调整，关键约束是各计算模块必须支持 II=1。

## 到 RTL 还需要补的点

1. 每个关键计算单元支持 `II = 1`

包括：

```text
MAC
EXP
reciprocal / x^-1
add
max
```

这些模块 latency 可以不同，但必须每拍能接收新输入。

2. fanout 和分级累加

当 `head_d = 128` 时，部分 MAC / reduction 结构可能需要分级，否则时序压力会比较大。

3. 数据类型选择

后续可能需要混合使用：

```text
BF16 / FP16 / FP32
```

中间计算是否需要额外打拍，也要结合资源和精度需求确定。

4. 外层系统逻辑

当前重点是：

```text
SRAM -> calculator -> SRAM
```

DRAM 到 SRAM 的搬运、tile 调度、host 驱动还没有完整纳入，需要后续补。

5. SRAM/BRAM width，目前看来至少是16 * 128 width，不知道是否合理

6. UI debug

后续 UI 可以增加 bit / hex 显示模式，方便和 RTL 波形对照。

## 资源估计

估计前提：

```text
1 个 MAC = 30 ALU + 30 FF + 1 DSP
工程路径: /d2/gongpuzhi/_fpga_pe
```

这里的 ALU 可以粗略理解成 FPGA 里的 ALM / LUT 逻辑预算。实际资源以综合报告为准，下表用于前期评估。

| 阶段 | 功能 | 可信度 | 硬件 latency 估计 | ALU 估计 | FF 估计 | DSP 估计 | 备注 |
|---|---|---:|---:|---:|---:|---:|---|
| K/V Delay | `K, V` 从 BRAM 中加载和打拍 | 中 | / | 3000 | 6000 | 0 | BRAM 存储，两个 ptr 读写 |
| MUX | `S, new_O` 的选择 | 中 | 2 | 2000 | 5000 | 0 | 暂按两级 MUX 估计 |
| QK MAC 阵列 | `S = QK^T` | 高 | 11 | 4000 | 4000 | 128 | 使用 MAC IP |
| PV MAC 阵列 + New O 复用 | `local_O = P V`，`new_O` 更新 | 高 | 11 | 4000 | 4000 | 128 | 使用 MAC IP，`new_O` 尽量复用 PV lane |
| MAX | `local_m = rowmax(S)` | 高 | 4 | 30 | 30 | 0 | 可用 IP |
| SUB | `N = S - new_m`，`a = old_m - new_m` | 高 | 4 | 30 | 30 | 0 | 可用 IP |
| EXP | `P = exp(N/sqrt(d))`，`b = exp(a/sqrt(d))` | 低 | 30 | 5000 | 5000 | 0 | 特殊函数最不确定，但仍要求 II=1 |
| Local L | `local_l = rowsum(P)` | 高 | 4 | 30 | 30 | 1 | 可用流水 accumulator / IP |
| New L | `new_l = old_l*b + local_l` | 高 | 11 | 30 | 30 | 1 | 使用 MAC IP |
| Reciprocal | `1 / new_l` | 低 | 30 | 3000 | 3000 | 1 ~ 4 | 特殊函数流水 IP，仍要求 II=1 |
| Final O | `O = new_O / new_l` | 高 | 7 | 30 | 30 | 1 | 使用 multiplier / MAC IP |
| Other | Q/K/V/O buffer、valid、index、clear、FSM | 中 | 1 ~ 4 | 1000 | 1000 | 0 | 包含同步 clear、tile 控制和状态机 |

整体资源粗略估计：

| 类型 | Conservative 估计 |
|---|---:|
| ALU | ~22k |
| FF | ~28k |
| DSP | 260 ~ 263 |
| BRAM / M20K | 容量约 64 起，考虑 banking 后约 128 ~ 260 |

结论：这个规模下，每个资源占用不超过 3%，BRAM资源可能吃紧
