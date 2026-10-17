# 09 · V4L2 实现归属：NVIDIA 栈 vs 厂商驱动

> 一句话结论：**只有 `sgx-yuv-gmsl2.ko` 是"V4L2 意义上的 sensor 驱动"；`max96712.ko` 完全不在 V4L2 里；
> 其余全部 V4L2 接口（video 节点、vb2、NVCSI/CSI subdev、媒体图、tegracam 框架）都由 NVIDIA 实现。**
>
> 标注：**【代码 file:line】** 源码实证 · **【DT】** 设备树实证 · **【实测】** 板上验证 · **【待核】** 未直接验证

---

## 1 · 三层分工一图看清

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ NVIDIA 提供（框架 + Tegra 硬件驱动，全部编进 tegra-camera.ko 等）             │
│                                                                             │
│  /dev/videoN ── video_device + v4l2_file_operations + v4l2_ioctl_ops         │
│      │              vi/channel.c                                            │
│      ├── vb2_ops（队列/起停流）  tegra_channel_queue_qops           :1027   │
│      ├── VI5 引擎 + RTCPU(IVC)   vi/vi5_fops.c + fusa-capture/*     :509    │
│      ├── media graph（entity/pad/link）  vi/graph.c                  :48    │
│      ├── CSI/NVCSI v4l2_subdev   csi/csi.c(:368) + nvcsi/csi5_fops.c        │
│      └── tegracam 框架：subdev 注册 + ctrl + async                          │
│            tegracam_v4l2.c:209（v4l2_i2c_subdev_init / v4l2_async_register_subdev）
│  nvhost-nvcsi-t194.ko ── 绑定 DT 节点 nvcsi@15a00000（nvidia,tegra194-nvcsi）│
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ tegracam 回调：parse_dt / power_on / set_mode / start_streaming …
┌───────────────────────────────▼─────────────────────────────────────────────┐
│ 厂商提供（森云 SGX / 研华）                                                  │
│                                                                             │
│  sgx-yuv-gmsl2.ko ── 填 camera_common_sensor_ops（:806-817）                │
│    · sensor_power_on/off    · sensor_set_mode（模式表）                      │
│    · sensor_start/stop_streaming（:686 / :712）                             │
│    · sensor_parse_dt（der_id / reg(=ser_addr) / 模式数）                     │
│    · 起流/停流时写串行器、解串器寄存器（经下面的库）                          │
│                                                                             │
│  max96712.ko ── 纯 i2c 驱动（**0 行 V4L2 代码**）：                          │
│    · regmap + debugfs + 导出 max96712_write/read_reg_Dser 等                 │
│    · global_priv[4]，被 sgx 直接当"函数库"调用                               │
└─────────────────────────────────────────────────────────────────────────────┘
                                │ I2C → 解串器 → GMSL → 串行器 → sensor
                                ▼
                          硬件（GMSL2 链路）
```

---

## 2 · 逐项归属矩阵（最实用的一张表）

| 功能/接口 | 实现方 | 模块 | 证据 |
|---|---|---|---|
| `/dev/videoN` 节点 | **NVIDIA** | `tegra-camera.ko`（`vi/channel.c`） | `video_device_alloc` :2562、`ioctl_ops` :2598 |
| `v4l2_file_operations` | **NVIDIA** | 同上 | `tegra_channel_fops` :2413 |
| `v4l2_ioctl_ops` | **NVIDIA**（缓冲类用 vb2 现成实现） | 同上 | `tegra_channel_ioctl_ops` :2304，`vb2_ioctl_*` :2314-2322 |
| `vb2_ops`（队列/起停流） | **NVIDIA** | 同上 | `tegra_channel_queue_qops` :1027 |
| VI5 抓帧 + RTCPU 协作 | **NVIDIA** | `vi/vi5_fops.c` + `fusa-capture/capture-vi.c` | `vi_capture_request` :477 / `status` :531 |
| NVCSI 设备（DT `nvcsi@15a00000`） | **NVIDIA** | **`nvhost-nvcsi-t194.ko`** | 【DT】`live.dts:7361-7368`；of_match `nvidia,tegra194-nvcsi` |
| CSI 核心 subdev（`s_stream` 等） | **NVIDIA** | `tegra-camera.ko`（`csi/csi.c`） | `tegra_csi_s_stream` :368 |
| csi5 具体实现（函数表） | **NVIDIA** | `tegra-camera.ko`（`nvcsi/csi5_fops.c`） | `struct tegra_csi_fops csi5_fops` :549；`csi5_start_streaming` :457 |
| 媒体图建链 | **NVIDIA** | `vi/graph.c` | `tegra_vi_graph_build_one/links` :48/:177 |
| sensor subdev 注册 / ctrl / async | **NVIDIA（tegracam）** | `tegracam_v4l2.c` | `tegracam_v4l2subdev_register` :209，内部 `v4l2_i2c_subdev_init` / `tegra_media_entity_init` / `v4l2_async_register_subdev` |
| **sensor 硬件动作**（电源、模式表、起停流） | **厂商 SGX** | **`sgx-yuv-gmsl2.ko`** | `camera_common_sensor_ops` :806-817；`:686/:712` |
| **GMSL/串行器/解串器寄存器操作** | **厂商** | **`sgx`（调用 `max96712` 导出函数）** | `sensor_start_streaming` 里 `max96712_write_reg_Dser(...)` :696 |
| 解串器驱动本体（regmap/debugfs/global_priv） | **厂商 研华** | **`max96712.ko`** | 全文 **0** 处 `v4l2/subdev/vb2/media_` 引用 |
| 板级 DT/模式参数 | **厂商** | overlay dtbo + `tegra-camera-platform` | `live.dts:5513+` |
| **NVCSI 时钟（214.3 MHz）** | **框架有接口、无人自动设** → 需自己加 | BPMP debugfs | 【实测】不设会 `corr_err` |

---

## 3 · 关键结论（四条）

### ① `max96712.ko` **不在 V4L2 体系里**
它不注册 video_device、不注册 subdev、不碰 vb2 —— 只是一个 **i2c + regmap + debugfs** 驱动，
通过 `EXPORT_SYMBOL` 给 `sgx-yuv-gmsl2` 当"寄存器访问库"用。
> 因此它**不受 V4L2 的 async/顺序机制保护** —— 这正是我们踩到 NULL panic 的结构性原因：
> `sgx` 在**自己的 probe 里**直接调用 `max96712` 的导出函数，而此时 `max96712` 可能还没 probe。

### ② `sgx-yuv-gmsl2.ko` 是唯一"厂商写的 V4L2 部分"，而且它只填**回调**
它不创建 video 节点、不管理 buffer，只实现 NVIDIA 定的 `camera_common_sensor_ops`
（`parse_dt / power_get / power_on / power_off / set_mode / start_streaming / stop_streaming`）
→ 由 `tegracam_device_register()` + `tegracam_v4l2subdev_register()` 把它接进 V4L2。

### ③ NVCSI 由**两个组件**共同实现
- `nvhost-nvcsi-t194.ko`：**绑 DT 设备** `nvcsi@15a00000`（`nvidia,tegra194-nvcsi`）【DT+of_match】
- `tegra-camera.ko` 内 `csi/csi.c`（CSI 核心 v4l2_subdev）+ `nvcsi/csi5_fops.c`（`tegra_csi_fops` 函数表）
两者的具体分工（谁管时钟/寄存器、谁管 subdev）**【待核】**，但都是 NVIDIA 代码。

### ④ 出图链上调用的每一层都能标出"找谁"
见第 4 节。

---

## 4 · 出问题找谁（责任矩阵）

| 症状 | 首查归属 | 具体位置 | 说明 |
|---|---|---|---|
| 开机 panic / NULL 解引用 | **厂商**（`max96712`+`sgx` 的顺序依赖） | `max96712.c` 三个导出函数 | 已由本仓库 `36c47474b` 修复（NULL 保护 + softdep） |
| 模块加载了但永不 probe（无绑定） | **厂商** | `max96712` 的 `i2c_device_id` 别名 | 研华预编译件只有 `i2c:max96712`，与 DT 客户端名 `max96712_1` 不匹配 |
| `can't find DER adapt broad ...` | **厂商** | `sgx` 的 `dax/dser_link_check` | DER 未就绪或链路未锁 |
| GMSL 链路不锁（`link` 值异常） | **厂商** | `sgx` dser_link_check 的寄存器读写 | 模组/线缆/`GMSLMODE` 配错 |
| `corr_err: discarding frame` / 超时 | **NVIDIA 框架 + 参数** | NVCSI 时钟、`csi5` 配置 | 本板需手动把 `nvcsi` 时钟设 214.3 MHz |
| `DQBUF` 卡住 / `vi_capture_status` 超时 | **NVIDIA** | `vi/vi5_fops.c`、`capture-ivc` | RTCPU/IVC 或 CSI 没数据 |
| `/dev/videoN` 不存在 | **NVIDIA 注册 或 DT** | `vi/channel.c`、overlay 是否合并 | 也查 `media-ctl -p` 是否有实体 |
| 格式/分辨率不生效、`sensor_mode` 无效 | **厂商 + DT** | `sgx` `set_mode`、`modeX` 参数 | 本板整数：`sensor_mode=1`+1920×1536 才通 |
| 帧率/时间戳异常 | **NVIDIA** | VI5/CSI 时钟、`sequence` | 看 `vbuf->sequence` 是否跳号 |
| 想加多相机同步（fsync） | **厂商** | `camera_fsync_set.sh` + 串行器 MFP7 | 本板启用会停流，需逐项验证 |

---

## 5 · 为什么"通用框架"没能防住我们的顺序问题（修正补充）

`v4l2_async` 解决的是 **subdev ↔ 驱动注册顺序**（`tegracam_v4l2.c:269` 确实用了它）。
但我们踩的坑不在 V4L2 层：

```
sgx_yuv_gmsl2_probe()            ← V4L2 subdev 还没注册完
   └─ max96712_write_reg_Dser()  ← 直接调【非 V4L2】的导出函数
        └─ global_priv[1] == NULL → 厂商代码 dev_err(&NULL->dev) → panic
```
**async 无法给"驱动 A 的导出函数"和"驱动 B 的 probe"排序** —— 这就是为什么最终要靠
`softdep sgx-yuv-gmsl2 pre: max96712`（加载顺序）+ 驱动内 NULL 保护（兜底）。

**通用改法（若要根治顺序依赖）**：把 `sgx` 在 probe 里的硬件初始化推迟到 `s_stream`（已基本如此）
或改为 `-EPROBE_DEFER` 让内核重试 —— 但那要处理 `tegracam` 注册后的回滚，风险较高（见 04 章）。

---

## 6 · 依据清单

| 类别 | 文件/行 |
|---|---|
| NVIDIA 框架与实现 | `camera/vi/channel.c:1027,2304,2413,2562,2598`；`vi/vi5_fops.c:460,509,531`；`vi/graph.c:48,177`；`csi/csi.c:368`；`nvcsi/csi5_fops.c:457,549`；`tegracam_core.c:119-186`；`tegracam_v4l2.c:209-269` |
| NVIDIA 设备驱动 | `drivers/video/tegra/host/nvcsi/{nvcsi-t194.c,nvcsi.c}`；`fusa-capture/capture-vi.c:1364,1425` |
| 厂商驱动 | `i2c/MIC-MAX96712/sgx-yuv-gmsl2.c:686,712,806-817,934`；`max96712.c`（无 V4L2） |
| DT | `live.dts:5513-5535`（解串器+cam_0）、`live.dts:7361-7368`（nvcsi@15a00000） |
| 相关章节 | 02（软件栈）、05（排障）、06（拓扑）、07（读数据链）、08（V4L2 框架） |
