# FlashAttention RTL-Like 设计说明

## 为什么需要 C Model 和 UI

从验证/调试角度看，Flash Attention 系统难调：

> **数据不直观**：浮点数据不如整数直观；MAC、EXP、Reciprocal 本身就是抽象算子。
>
> **时序关系复杂**：不同计算 stage 之间的时间关系比较复杂。
>
> **C Model 更适合快速迭代**：目前来说，AI 更擅长生成/调试 C 代码；只要给上 RTL-like 的约束，就比纯 RTL 效率高。

## 硬件架构设计思路

常见做法一般有两类：

> **GPU / TPU / NPU 风格的 kernel 或 command 调度**：优点是通用性强，但需要硬件调度器、软件编译/调度配合，软硬件耦合更复杂。
>
> **HLS / FPGA pipeline accelerator**：开发快，但每个周期的数据位置和对齐关系不够直观，后期 debug 成本较高。

目前这个设计选择更直接的全流水 RTL 数据流：

- 牺牲一部分可扩展性 
- 换取更高资源利用率 
- 换取更简单的硬件控制 
- 换取更直接的验证路径 

## 当前计算粒度

关注的是 LLM decode 阶段中的 attention 计算。粒度逐步缩小为：

- 一个 batch
- 一个 decode step
- 一个 transformer layer
- 一个 attention head
- 一个 attention tile

目前设定：

```text
head_d = 128
tile = 128
```

所以，
如果输入 token 数是 4096，那么按 128 为 tile 粒度，需要计算 32 次 tile，每个 tile 的大小为：

```text
Q: 128
K: 128 x 128
V: 128 x 128
O: 128
```

## 数据流结构

整体结构可以概括为：

```text
SRAM + handshake -> calculator pipeline -> SRAM
```

calculator pipeline 内部是全流水结构，大致如下：

```text
QK -> S -> P -> PV -> O
```

粗略性能目标：

```text
模块内部 II = 1
tile-level II ~= 128 + 1 cycles ~= 300 cycles
单 tile latency ~= 6 * 128 + 100 ~= 870 cycles
```

其中 latency 可以通过打拍调整，关键约束是各计算模块必须支持 II=1。

## 到 RTL 还有多远

### 关键计算单元支持 `II = 1`

包括：

- MAC
- EXP
- reciprocal / x^-1
- add
- max

这些模块 latency 可以不同，但必须每拍能接收新输入（就是 II 启动间隔为 1）。

### MUX 问题

当 `head_d = 128` 时，MUX fan-in 较大，可能有时序压力。

### BRAM Width 问题

当 `head_d = 128` 时，比如说 K_SRAM, V_SRAM 是 16 * 128 width，不知道是否合理。

### 数据类型 问题

BF16 / FP16 / FP32，可能需要混合使用，目前只考虑 FP32

### 外层系统逻辑

当前重点是：

```text
SRAM -> calculator -> SRAM
```

DRAM 到 SRAM 的搬运、tile 调度、host 驱动还没有完整纳入。

### UI debug

后续 UI 可以增加 bit / hex 显示模式，方便和 RTL 波形对照。

### FPGA 硬件资源估计

估计前提如下，intel 板卡 `I-Series AGIB027R29A1E2V` 上有 MAC 相关的硬件 IP：

```text
1 个 MAC = 30 ALU + 30 FF + 1 DSP
工程路径: 03服务器：/d2/gongpuzhi/_fpga_pe
```

| 阶段 | 功能 | 可信度 | 硬件 latency 估计 | ALU 估计 | FF 估计 | DSP 估计 | 备注 |
|---|---|---:|---:|---:|---:|---:|---|
| K/V Delay | `K, V` 从 BRAM 中加载和打拍 | 中 | / | 3000 | 6000 | 0 | 如果打拍路径太长了，可以用 BRAM 存储，两个 ptr 读写 |
| MUX | `S, new_O` 的选择 | 中 | 2 | 2000 | 5000 | 0 | 暂按两级 MUX 估计 |
| QK MAC 阵列 | `S = QK^T` | 高 | 11 | 4000 | 4000 | 128 | 使用 MAC IP |
| PV MAC 阵列 + New O 复用 | `local_O = P V`，`new_O` 更新 | 高 | 11 | 4000 | 4000 | 128 | 使用 MAC IP，`new_O` 复用 PV lane |
| MAX | `local_m = rowmax(S)` | 高 | 4 | 30 | 30 | 1 | 可用 IP |
| SUB | `N = S - new_m`，`a = old_m - new_m` | 高 | 4 | 30 | 30 | 1 | 可用 IP |
| Local L | `local_l = rowsum(P)` | 高 | 4 | 30 | 30 | 1 | 使用 accumulator IP |
| New L | `new_l = old_l*b + local_l` | 高 | 11 | 30 | 30 | 1 | 使用 MAC IP |
| Final O | `O = new_O / new_l` | 高 | 11 | 30 | 30 | 1 | 使用 MAC IP |
| EXP | `P = exp(N/sqrt(d))`，`b = exp(a/sqrt(d))` | 低 | 30 | 5000 | 5000 | 0 | 要求 II=1，未调研 |
| Reciprocal | `1 / new_l` | 低 | 30 | 3000 | 3000 | ~ | 要求 II=1，未调研 |
| Other | Q/K/V/O buffer、valid、index、clear、FSM | 中 | / | 1000 | 1000 | 0 | 包含同步 clear、tile 控制和状态机 |

整体资源粗略估计：

| 类型 | Conservative 估计 |
|---|---:|
| ALU | ~22k |
| FF | ~28k |
| DSP | 261 |

结论：这个规模下，每个资源占用不超过 3%，BRAM 暂无评估，可能吃紧
