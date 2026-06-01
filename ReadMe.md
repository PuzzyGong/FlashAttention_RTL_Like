# FlashAttention RTL-Like 设计说明

## WHY need `C model` 和 `UI`

验证/测试角度：
1. 浮点难调试
2. MAC、EXP、reciprocal 本身就是抽象算子
3. 全流水结构组合后更复杂

# 我的思考流程

## 和常见硬件加速方式的区别

目前比较通用的做法是： GPU / TPU / NPU 风格的 kernel 或 command 调度。1. 要硬件调度器 2. 软件的编译过程也复杂 3. 但软硬件耦合也更复杂。


第二种是 HLS / FPGA pipeline accelerator。这种方式开发快，但底层每个周期的数据位置和对齐关系不够直观，后期调试会比较困难。

目前这种全流水的设计：
```text
牺牲一部分可扩展性
换取更高资源利用率
换取更简单的硬件控制
换取更高开发和验证效率
```

## 当前计算粒度

从推理阶段的

粒度缩紧：
```text
一个 batch
一个 decode step / token output
一个 transformer layer
一个 attention head
一个 attention tile
```

目前设定 `head_d = 128` ，一个 tile 可以看成：

```text
Q: 128
K: 128 x 128
V: 128 x 128
```

如果一次 FlashAttention 的输入 token 数是 4096，那么可以按 128 为 tile 粒度拆成 32 次计算。

## 数据流结构

数据流可以概括为：

```text
SRAM + handshake握手 -> calculator pipeline -> SRAM
```

其中 calculator pipeline 内部是全流水结构，大致包括：

```text
QK -> S -> P -> PV -> O
```

## 从目前项目到 RTL 还有多远

为了让全流水结构成立，当前 RTL 设计需要满足一些基本假设。

0. model UI部分，后续可以增加bit hex的读模式。这样才能方便调试

1. 每个关键计算单元都要支持 `II = 1`

包括：

```text
MAC
EXP
reciprocal / x^-1
add
max
```

这些单元可以有不同 latency，但必须保证启动间隔为 1。

2. 处理 fanout 和分级累加

 `head_d` == 128时，真正迁移到 RTL 时，部分 MAC / reduction 结构可能需要分级。
 那不然时序通不过

3. 数据类型可能需要混合

如果资源或时序压力较大，部分数据类型可能需要转换。例如：

```text
BF16 / FP16 / FP32
```

中间计算过程也可能需要额外打拍。这部分目前还没有完全建模。


4. 设计上 sram -> caculator -> sram 通路，dram -> 这部分没有，这里要有调度逻辑 + 驱动






## 资源估计

下面的估计基于一个偏保守的假设：

```text

1 个 MAC = 30 ALU + 30 FF + 1 DSP 

3 服务器 工程路径 /d2/gongpuzhi/_fpga_pe
```

这里的 ALU 可以粗略理解成 FPGA 里的 ALM / LUT 逻辑预算。实际资源需要以 Quartus 综合报告为准，下表主要用于前期评估。

| 阶段 | 功能 | 可信度 | 硬件latency估计 | ALU 估计 | FF 估计 | DSP 估计 | 备注 |
|---|---|---:|---:|---:|---:|---:|---|
| K/V Delay | `K, V` 从 BRAM 中加载和打拍 | 中 | / | 3000 | 6000 | 0 | 用BRAM来存储，两个ptr读写数据 |
|  MUX | `S, new O`的MUX | 中 | 2 | 2000 | 5000 | 0 |  感觉两级MUX足矣
| QK MAC 阵列 | `S = QK^T` | 高 | 11 | 4000 | 4000 | 128 | 用 MAC IP, 这个是确定的，浮点加法和乘法都有Hard IP，
| PV MAC 阵列 + New O 复用| `local_O = PV` | 高 | 11 | 4000 | 4000 | 128 | 用 MAC IP, 这个是确定的 |
| MAX | `local_m = rowmax(S)` | 高 | 4 | 30 | 30 | 0 | 可用 IP |
| SUB | `N = S - new_m`，`a = old_m - new_m` | 高 | 4 | 30 | 30 | 0 | 可用 IP |
| EXP | `P = exp(N/sqrt(d))`，`b = exp(a/sqrt(d))` | 低 | 30 | 5000 | 5000 | 0 | 特殊函数 latency 最不确定，但仍要求 II=1 |
| Local L | `local_l = rowsum(P)` | 高 | 4 | 30 | 30 | 1 | 可用 IP |
| New L | `new_l = old_l*b + local_l` | 高 | 11 | 30 | 30 | 1 | 用 MAC IP  |
| Reciprocal | `1 / new_l` | 低 | 30 | 3000 | 3000 | 1 ~ 4 | 特殊函数流水 IP，仍要求 II=1 |
| Final O | `O = new_O / new_l` |  高 | 7 | 30 | 30 | 1 | 用 MAC IP  |
| Other | Q/K/V/O buffer   valid / index / clear / FSM | ~ | 1 ~ 4 | 1000  | 1000  | 0 | 包含同步 clear、tile 控制和状态机 |

整体资源可以粗略估为：

| 类型 | Conservative 估计 |
|---|---:|
| ALU | 
| FF | 
| DSP | 
| M20K | 





