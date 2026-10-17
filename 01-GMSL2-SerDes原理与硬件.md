# 第 1 章 GMSL2 SerDes 原理与本板硬件

> 适用平台：MIC-712AR（Advantech 载板 + NVIDIA Jetson Orin NX，L4T R36.4.3 / JetPack 6.2，内核 `5.15.148-tegra`）
> 相机模组：Sensing **SGX-YUV-GMSL2** ×4，解串器 **MAX96712**，串行器 **MAX96717**（模组内）
> 本章只讲**原理 + 本板硬件与代码事实**，出图流程/排障见后续章节。

## 标注约定（全文遵守）

| 标记 | 含义 |
| --- | --- |
| 【实测】 | 本会话在该板上真实测到（dmesg / sysfs / i2ctransfer / v4l2-ctl 输出） |
| 【代码依据】 | 直接来自仓库源码，附文件与行号，可自行核对 |
| 【脚本原文】 | 研华脚本里的原样寄存器写，含义未在代码中给出 |
| 【推断】 | 由代码/现象推理得出，**未经 datasheet 或实验确认** |
| 【待验证】 | 明确不知道，留给后续章节或厂商确认 |

---

## 本章结论速览

1. **本板是"4 路 GMSL2 同轴 + 1 颗 4 路解串器"的星型结构**：4 颗 MAX96717（串行器，随模组）各接 1 个 sensor，通过 4 根同轴线进 1 颗 MAX96712（解串器，在载板上），解串器再以 **4 路 CSI-2（每路 4 lane）** 送进 Orin NX 的 NVCSI。解串器挂在 **i2c-2（`i2c@3180000`）地址 0x29**。【实测】
2. **GMSL 的本质是"一条同轴线同时承担视频下行 + I2C 控制双向 + 供电(PoC)"**，因此 SoC 侧只需 1 根线/相机就能完成"取流 + 配置寄存器"，这正是它取代并行 DVP/多芯线缆的原因。
3. **代码里 GMSL 速率档由模块参数 `GMSLMODE_<der_id>` 逐路给出**（【代码依据】`sgx-yuv-gmsl2.c:231-240`）：`0 = GMSL1`，`非 0 = GMSL2`；其中 **`1` 在代码里只体现为"不去改 0x0010/0x0011 的那两个位"**，`2` 是本板在用档位（研华注释：1=GMSL2/6G、2=GMSL2/3G）。本板配 **`GMSLMODE_1=2,2,2,2`**。【实测】
4. **链路锁定判据在代码里只有一个位**：`dser_link_check()` 先写 `0x0006` 打开该路 I2C 透传，再读该路的链路状态寄存器（GMSL2 用 `0x001A/0x000A/0x000B/0x000C`），**`link & 0x8` 为真即视为锁定**（【代码依据】`sgx-yuv-gmsl2.c:298-327`）。**`0x8`/`0x80` 的具体 datasheet 位定义代码里没有，属【待验证】。**
5. **本板实测四路 link 值为 `0xDA / 0xC8 / 0xC8 / 0xC8`**，四路 bit3 与 bit7 都置位；`max96712_set_lane_rate()` 用 bit7 统计出 **`link_num = 4`**，与实测日志一致。【实测 + 代码依据】
6. **"分辨率"在本驱动里不改变传感器**：`mode_table[]` 的 0~4 号（1920×1080 / 1920×1536 / 2880×1860 / 3840×2160 / 1280×720）**全部指向同一张空表 `sensor_3840x2160_crop_30fps`**（【代码依据】`sgx-yuv-gmsl2-tbls.h:122-124, 452-457`），真正决定几何的是设备树里各 `modeX` 的时序/裁剪参数。**本板实测只有 `sensor_mode=1`（1920×1536）能出图，`sensor_mode=0`（1920×1080）会挂住**，根因未定，见 §1.6.4【待验证】。
7. **帧同步（fsync）是可选功能，且在本模组上有风险**：研华 `camera_fsync_set.sh` 会给解串器写一组 GPIO/MFP 寄存器并使能 PWM，再改写 4 颗串行器的 MFP7(`0x02D3`)。**本板实测正常运行（未执行该脚本）时 MFP7 = `0x18`**，而脚本会把它写成 `0x84/0x85`；上一轮排查中应用 fsync 后**相机停流**，故当前流程不启用 fsync。【实测 + 脚本原文，结论【推断】】
8. **本板不需要为"相机"额外配置时钟以外的东西**：CSI 侧 4 lane / UYVY 16bit / vc_id 0~3 / embedded_metadata 高度 0 全部由设备树给定（【代码依据】`live.dts` 相机节点），MVCSI 时钟需 214.3 MHz 才能稳定出图（见后续章节）。

---

## 1.1 为什么车载/工业相机用 GMSL

### 1.1.1 传统方案的三个硬伤

| 方案 | 线缆 | 传输距离 | 典型问题 |
| --- | --- | --- | --- |
| DVP/并口（8~16bit + 行场时钟） | 十几根并行线 | < 0.3 m | 走线长一点就时序崩塌；EMI 大；线束重 |
| MIPI CSI-2 直连 | 4~8 对差分 | < 0.3~0.5 m | 无法出机箱；连接器贵；无供电 |
| **GMSL/车载 SerDes** | **1 根同轴（或 1 对差分）** | **10~15 m** | 需要 SerDes 芯片；链路需配置/锁定 |

车载/工业场景的相机经常离 SoC 数米（车顶、机械臂、龙门），还要过电机/继电器等强干扰环境，并且希望**一根线同时解决视频、控制、供电**。GMSL 就是为这个设计的。

### 1.1.2 GMSL 的四个关键特性

| 特性 | 说明 | 本板体现 |
| --- | --- | --- |
| **串行同轴** | 视频、控制、状态复用到一条链路上，按帧/包传输 | 每路相机 1 根同轴进载板 |
| **长距离** | 线缆有均衡/去加重（deskew）能力，可达 10 m 以上 | 【代码依据】`sgx-yuv-gmsl2.c:618-623` 会写 `0x903/0x943/0x983/0x9c3 = 0x80`，注释写 `//descrew`（去加重/均衡相关） |
| **抗干扰** | 差分/同轴传输 + 链路 CRC/重传，误码以"链路重传"体现而非直接花屏 | 链路状态寄存器可读（§1.3） |
| **PoC（Power over Coax）** | 供电与信号叠加在同一根同轴上，相机侧无需单独电源线 | 载板侧把 PoC 电感/滤波做在硬件上；软件侧只需要控制相机 `reset-gpios`（【代码依据】`live.dts:5527`） |

> 【推断】"PoC 由硬件实现"这一点从软件侧只能看到 reset-gpios 与电源轨；具体 PoC 滤波网络属于原理图范围，本仓库无资料 → 【待验证】。

### 1.1.3 Serializer 与 Deserializer 的分工

| 角色 | 芯片（本板） | 位置 | 对上/对下接口 | 干什么 |
| --- | --- | --- | --- | --- |
| **Serializer（串行器）** | **MAX96717**（在模组内） | **sensor 侧**（相机壳体内） | 上：MIPI CSI-2（来自 sensor）／下：GMSL 同轴 | 把 sensor 的并行/CSI 数据打包成 GMSL 链路帧；同时把 SoC 发来的 I2C 透传到 sensor |
| **Deserializer（解串器）** | **MAX96712**（在载板上） | **SoC 侧** | 上：4×CSI-2 到 NVCSI／下：4 路 GMSL 同轴 | 解出 4 路视频并分发到 4 个 CSI-2 端口；把 SoC 的 I2C 请求按"链路选择"透传到对应串行器；产生/接收帧同步信号 |

【代码依据】这一分工在代码里体现为两个不同的"地址空间"：

```c
/* sgx-yuv-gmsl2-tbls.h:28-29 */
#define MAX96712_ADDR  0x52   // Update to 0x29 << 1 == 0x52   ← 解串器（8bit 形式）
#define MAX96717_ADDR  0x80                                   ← 串行器（默认地址的 8bit 形式）
```

驱动访问任何芯片统一走解串器的 I2C（`max96712_write_reg_Dser()` 的第一个参数 `slaveAddr` 是 **8bit 地址**，函数内部 `i2c_client->addr = slaveAddr / 2` 再交给 i2c 核心 → 【代码依据】`max96712.c:36-51`、`sgx-yuv-gmsl2.c:242-285`）。

---

## 1.2 前向链路、反向链路与速率等级

### 1.2.1 一条同轴上的三个"通道"

| 方向 | 名称 | 承载内容 | 本板用途 |
| --- | --- | --- | --- |
| **下行**（SoC → 相机） | 前向链路（forward） | **视频数据**（高速串行） | sensor 图像 → 串行器 → 同轴 → 解串器 → CSI-2 → NVCSI |
| **上行**（相机 → SoC） | 反向链路（backward / control） | **控制与状态**（低速、与视频共线） | sensor/串行器寄存器读写（I2C over GMSL）、链路锁定状态 |
| **反向（I2C 下行）** | I2C passthrough | SoC 的 I2C 请求被解串器转运到指定链路的串行器 | 配 sensor、写 MFP、做 fsync（§1.5、§1.7） |

> 说明：GMSL 的 "forward" 指高速视频方向（SoC 接收），"backward" 指低速控制方向；I2C 请求由 **SoC 发起**、经解串器下行到串行器，这一点容易与"反向通道"混淆。本文按"谁发起"描述，避免歧义。

### 1.2.2 速率等级与代码里的 `GMSLMODE`

| 等级 | 每 link 速率（量级） | 代码里的取值 | 备注 |
| --- | --- | --- | --- |
| **GMSL1** | ~1.5 / 3 Gbps（老一代） | **`GMSLMODE = 0`** | 【代码依据】走 GMSL1 分支：链路寄存器组换成 `0x0BCB/0x0CCB/0x0DCB/0x0ECB`，锁定判据变成 `link & 0x90` |
| **GMSL2 / 6G** | 6 Gbps | **`GMSLMODE = 1`**（研华注释） | 【推断】代码里 `1` 只体现在 `if (priv->GMSLMODE[i] != 1)` 的取反分支（即"不动 `0x0010/0x0011` 中的对应位"） |
| **GMSL2 / 3G** | 3 Gbps | **`GMSLMODE = 2`** | 本板在用（模组是 3G 器件）；仍走 GMSL2 分支 |

【代码依据】三处关键代码：

```c
/* (1) 定义与暴露：sgx-yuv-gmsl2.c:230-240 —— 每路一个值，且只读(S_IRUGO) */
static int n_para = 4;
static int GMSLMODE_0[4] = {0,0,0,0};
static int GMSLMODE_1[4] = {0,0,0,0};
static int GMSLMODE_2[4] = {0,0,0,0};
static int GMSLMODE_3[4] = {0,0,0,0};
static int *GMSLMODE[4] = {GMSLMODE_0, GMSLMODE_1, GMSLMODE_2, GMSLMODE_3};
module_param_array(GMSLMODE_0, int, &n_para, S_IRUGO);
...
```

```c
/* (2) 按 der_id 选一组：sgx-yuv-gmsl2.c:895 */
memcpy(priv->GMSLMODE, GMSLMODE[priv->der_id], 4 * sizeof(int));
```

```c
/* (3) 逐路生效：sgx-yuv-gmsl2.c:744-796（max96712_init_mode）*/
if (priv->GMSLMODE[0] != 1) mode_phyab = (mode_phyab & ~0x3)       | 0x1;
if (priv->GMSLMODE[1] != 1) mode_phyab = (mode_phyab & ~(0x3<<4))  | (0x1<<4);
if (priv->GMSLMODE[2] != 1) mode_phycd = (mode_phycd & ~0x3)       | 0x1;
if (priv->GMSLMODE[3] != 1) mode_phycd = (mode_phycd & ~(0x3<<4))  | (0x1<<4);
max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0010, mode_phyab);
max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x0011, mode_phycd);
...
if (priv->GMSLMODE[i] == 0) {   /* GMSL1 路径：写 0x041A / 0x0415 / 0x0418 */
        max96712_write_reg_Dser(..., 0x041A, ((0x01 << 4) | val));
        max96712_write_reg_Dser(..., 0x0415, ((0x04 << 4) | val));
} else {                        /* GMSL2 路径：按位打开 0x0006 */
        max96712_read_reg_Dser (..., 0x0006, &val);
        max96712_write_reg_Dser(..., 0x0006, ((0x01 << 4) | val));
}
```

要点：

* `0x0010` / `0x0011` 低半字节管 link A/B，高半字节管 link C/D（`mode_phyab` / `mode_phycd`）——【代码依据】上段；**寄存器名/位定义未在代码注释中给出 → 【待验证】**。
* **`GMSLMODE = 0` 与 `非 0` 是两条完全不同的代码路径**：GMSL1 用 `0x041A/0x0415/0x0418`（软件覆写速率），GMSL2 用 `0x0006`（按位打开某路）。
* 【推断】厂商配置表注释佐证了 `0x0010/0x0011 = 0x22` 与 "work in 6G mode" 的关系（`sgx-yuv-gmsl2-tbls.h:51-52`）。
* **一个容易踩的点**：开机时真正写下去的是 **GMSL1 那张配置表**，而不是 6G 表——

```c
/* sgx-yuv-gmsl2.c:901-903 */
if (der_init_flag[priv->der_id] == 0) {
    err = i2c_write_table(priv, mode_table[MAX96712_GMSL1_CONFIG]);
    //err = i2c_write_table(priv, mode_table[MAX96712_GMSL2_6G_CONFIG]);
    ...
```
即：**"基础初始化"固定用 `max96712_gmsl1_tbl`，随后各路的 GMSL 档位再由 `max96712_init_mode()` 按 `GMSLMODE` 覆盖。**【代码依据】+【推断】（这是厂商代码现状，意图待确认）。

### 1.2.3 本板实际配置

【实测】板上 `/etc/modprobe.d/mic712-camera.conf`：

```
options sgx-yuv-gmsl2 GMSLMODE_1=2,2,2,2
```

| 项 | 值 | 说明 |
| --- | --- | --- |
| `der_id` | **1** | 设备树 `cam_*` 节点的 `der_id = <0x01>`（【代码依据】`live.dts:5524/5805/6086/6391`）→ 所以用 `GMSLMODE_1` |
| 四路档位 | `2,2,2,2` | 四路都是 GMSL2/3G |
| 生效时机 | **模块加载时**（`S_IRUGO` 只读，之后不能用 sysfs 改） | 改配置必须改 `modprobe.d` 并重新 `modprobe` |
| 核对命令 | `cat /sys/module/sgx_yuv_gmsl2/parameters/GMSLMODE_1` | 【实测】输出 `2,2,2,2` |

---

## 1.3 链路锁定判据与流程

### 1.3.1 `dser_link_check()` 的完整流程

【代码依据】`sgx-yuv-gmsl2.c:287-355`。按执行顺序拆解：

| 步 | 代码行 | 动作 | 寄存器/值 |
| --- | --- | --- | --- |
| 1 | 294 | 计算本路编号 | `cam_id = priv->id->driver_data % 4`（0~3，对应 link A~D） |
| 2 | 299 | 读"链路使能/透传选择"寄存器当前值 | 解串器 `0x0006` |
| 3 | 300 | 选本路链路状态寄存器地址 | GMSL2：`{0x001A, 0x000A, 0x000B, 0x000C}[cam_id]`（link A/B/C/D）<br>GMSL1：`{0x0BCB, 0x0CCB, 0x0DCB, 0x0ECB}` |
| 4 | 301 | **按位打开本路**：`0x0006 = 旧值 + (1 << cam_id)` | 【推断】bit N 对应 link N 的 I2C 透传/链路使能 |
| 5 | 306 | 等 200 ms 让链路稳定 | `msleep(200)` |
| 6 | 307 | 读该路链路状态 | `link` 变量 |
| 7 | 310 | **判据**：`if (link & 0x8)` | 真 → 认为锁定，继续配置；假 → `return 0`（= 未检测到） |
| 8 | 311-321 | 锁定后写串行器（MAX96717）寄存器：`0x02BE=0x10`、`0x02D6=0x10`、`0x0318=0x5E`、`0x02D3=0x00`、`0x02D6=0x00`、`0x02D3=0x10`、`0x02D6=0x10`、`0x0002=0x03`、**`0x0000 = 0x84 + cam_id*4`** | 最后一条是**给串行器改地址**（§1.5） |

GMSL1 分支（328-352）结构相同，但：锁定判据是 **`link & 0x90`**，且锁定前先通过 `sensor_write_reg()` 给串行器写 `0x04=0x47`、读 `0x00`，然后用 `0x07=0x84`、`0x0F=0xBE/BF`、`0x00=0x84+cam_id*4` 配置。**两套判据（`0x8` vs `0x90`）的位含义代码未给出 → 【待验证】。**

### 1.3.2 锁定判据的证据表

| 项 | GMSL2（本板在用） | GMSL1 | 来源 |
| --- | --- | --- | --- |
| 状态寄存器 | `0x001A`(A) / `0x000A`(B) / `0x000B`(C) / `0x000C`(D) | `0x0BCB` / `0x0CCB` / `0x0DCB` / `0x0ECB` | 【代码依据】`sgx-yuv-gmsl2.c:218-221` |
| 判据位 | `link & 0x8` | `link & 0x90` | 【代码依据】310/341 |
| 另一处用到的位 | `val & 0x80` → 计入 `link_num` | 同 | 【代码依据】`605-608` |
| **本板实测值** | **port0 = `0xDA`，port1/2/3 = `0xC8`** | — | 【实测】dmesg |
| 由实测值推出的位 | `0xDA = 1101_1010`、`0xC8 = 1100_1000` → **bit3 与 bit7 均为 1** | — | 【实测 + 推断】 |
| 与 `link_num` 一致性 | bit7 全为 1 → `link_num = 4`，与日志 `link_num = 4` 一致 | — | 【实测】+【代码依据】 |

【实测】原始日志（本会话，板子 dmesg）：

```
sgx-yuv-gmsl2 2-001a: dser_link_check Der-1 link port 0---0 device....
sgx-yuv-gmsl2 2-001a: dser_link_check link:0xda
+++> Der-1 port 0 camera sgx-yuv-gmsl2-0 been detected!
sgx-yuv-gmsl2 2-001b: dser_link_check link:0xc8
+++> Der-1 port 1 camera sgx-yuv-gmsl2-1 been detected!
sgx-yuv-gmsl2 2-001c: dser_link_check link:0xc8
+++> Der-1 port 2 camera sgx-yuv-gmsl2-2 been detected!
sgx-yuv-gmsl2 2-001d: dser_link_check link:0xc8
+++> Der-1 port 3 camera sgx-yuv-gmsl2-3 been detected!
```

**必须强调**：`0x8`、`0x80`、`0x90` 的**位定义来自 datasheet，本仓库代码里没有任何注释说明**，因此：

* 【代码依据】：这些位**被代码这样使用**（锁定/计数）；
* 【待验证】：它们**在 datasheet 里叫什么**；
* **不要**据 0xDA 与 0xC8 的低 4 位差异（`0xA` vs `0x8`）推断"port0 有什么特别"——【待验证】。

### 1.3.3 挂载链路检查的上层流程

【代码依据】`sgx-yuv-gmsl2.c:895-921`（`sensor_probe` 尾部）：

```c
memcpy(priv->GMSLMODE, GMSLMODE[priv->der_id], 4*sizeof(int));
sensor_power_on(priv->s_data);
msleep(100);
probe_count++;
if (der_init_flag[priv->der_id] == 0) {          /* 四路里只有第一路会进来 */
    err = i2c_write_table(priv, mode_table[MAX96712_GMSL1_CONFIG]);
    if (err)
        dev_err(&client->dev, "can't find DER adapt broad in port %s \n", id->name);
    else {
        der_init_flag[priv->der_id] = 1;
        max96712_init_mode(priv);                 /* ← §1.2 的 GMSLMODE 就在这里生效 */
        dev_info(&client->dev, "Init DER %d adapt broad success\n", priv->der_id);
    }
}
if (der_init_flag[priv->der_id] == 1) {
    if (dser_link_check(priv) == true)
        printk("+++> Der-%d port %ld camera %s been detected!\n", ...);
    else
        dev_err(&client->dev, "%s camera %s detect error\n", __func__, id->name);
}
```

**这张流程图很关键**，因为它是"整板 4 路相机能不能起来"的开关：

```
sensor_probe(第 1 路)  ──► 写 GMSL1 基础表 ──► 成功?
                                   │              ├─ 否 → 打印 "can't find DER adapt broad"
                                   │              │        且 der_init_flag 仍为 0
                                   └──────────────┴─ 是 → der_init_flag=1
                                                          max96712_init_mode()（GMSLMODE 生效）
                                                          后续 4 路各自 dser_link_check()
```

【重要】`i2c_write_table()` 的每一次寄存器写都会落到 `max96712_write_reg_Dser()`（【代码依据】`sgx-yuv-gmsl2.c:357-401`），而该函数依赖 **max96712 驱动已经 probe 过**（`global_priv[der_id]` 有值）。若 max96712 还没 probe，厂商代码在 `max96712.c:44-47` 用 **未赋值的 `i2c_client`** 去 `dev_err()`，会触发内核 NULL 解引用。**这是本板第 1 章必须记住的硬约束：加载顺序必须是 max96712 先、sgx-yuv-gmsl2 后，且 max96712 必须完成 probe。** 详细分析与修复见后续章节。

---

## 1.4 速率与通道：`link_num` / `lane_rate` 与相关寄存器

### 1.4.1 `max96712_set_lane_rate()`

【代码依据】`sgx-yuv-gmsl2.c:598-626`：

```c
for (i = 0; i < 4; i++) {
    max96712_read_reg_Dser(priv->der_id, MAX96712_ADDR, max96712_gmsl2_link_reg_tbl[i], &val);
    if ((val & 0x80) != 0)
        link_num++;                       /* ← 用 bit7 统计"有几路链路建立了" */
}
priv->link_num[priv->der_id] = link_num;

rate = 0xE0 + lane_rate + 2 * (priv->link_num[priv->der_id]);   /* ← 纯计算，未写回寄存器 */
err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x041B, 0xF7);   /* ← 唯一写入 */
dev_info(priv->tc_dev->dev, "link_num = %d ,lane_rate = 0x%x\n", priv->link_num[priv->der_id], rate);

/* 每个 link 的 CSI-2 通道"去加重/均衡"控制 */
val = 0x80;
err  = max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x903, val);
err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x943, val);
err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x983, val);
err += max96712_write_reg_Dser(priv->der_id, MAX96712_ADDR, 0x9c3, val);
```

要点：

* **`rate` 只是算出来打印用的**（`lane_rate = 0x%x` 打的就是它），**并没有写进任何寄存器** —— 这点很容易误读。
* 真正写下去的只有：`0x041B = 0xF7`（注释 `// Fixed set max`，即"固定设成最大值"）与 4 处 `0x903/0x943/0x983/0x9c3 = 0x80`（注释 `//descrew`）。

### 1.4.2 `lane_rate` 从哪来

【代码依据】`sgx-yuv-gmsl2.c:670`（在 `sensor_set_mode()` 里）：

```c
lane_rate = mode->signal_properties.serdes_pixel_clock.val * 16 / 100000000 / mode->signal_properties.num_lanes;
max96712_set_lane_rate(priv, lane_rate);
```

| 符号 | 来源 | 本板值 |
| --- | --- | --- |
| `serdes_pixel_clock.val` | 设备树 `modeX/serdes_pix_clk_hz` | mode1：`350000000`（【代码依据】`live.dts:5702`） |
| `num_lanes` | 设备树 `modeX/num_lanes` | `4` |
| 计算结果 | `350000000*16/100000000/4 = 14 = 0xE` | |
| `link_num` | §1.4.1 统计 | `4` |
| **`rate`** | `0xE0 + 0xE + 2*4 = 0xF6` | **【实测】日志 `link_num = 4 ,lane_rate = 0xf6`** ✔ 完全吻合 |

> 这条对得上说明：**设备树的 `serdes_pix_clk_hz=350 MHz` 与代码公式、板子实测三者自洽**。它给了一个很有用的自检点：如果改了 `serdes_pix_clk_hz` 或某一路链路没锁上，`lane_rate` 打印值会随之变化。

### 1.4.3 `0x0415 / 0x0418 / 0x041B / 0x041E` 的两种用法

| 出处 | 写法 | 说明 |
| --- | --- | --- |
| 【代码依据】`sgx-yuv-gmsl2.c:615` | `0x041B = 0xF7` | 注释 `// Fixed set max`（强制最大值） |
| 【代码依据】`sgx-yuv-gmsl2.c:758-792`（GMSL1 分支） | `0x041A = (bit<<4)\|val`、`0x0415/0x0418 = (bit<<4)\|val` | 逐路读改写，按 port 位域开/关 |
| 【代码依据 + 厂商注释】`sgx-yuv-gmsl2-tbls.h:113-116` | `0x0415/0x0418/0x041B/0x041E = 0x37` | 表内注释：`Set Data rate to be 2300Mbps/lane for port A/B ... and enable software override` |

**结论（谨慎表述）**：这几个寄存器**属于"每路数据速率/软件覆写"家族**（厂商注释如此），但：

* 本板实际生效值是多少（`0xF7` 还是被后续覆盖成别的）**需要读寄存器确认 → 【待验证】**；
* `2300 Mbps/lane` 与模组手册/实际链路速率的对应关系 **【待验证】**；
* 代码写的 `0x041B=0xF7` 与表的 `0x041B=0x37` **语义冲突**（同一寄存器、不同值、不同注释）→ **【待验证】，不要凭此断定速率**。

### 1.4.4 与 CSI-2 通道数的关系

* `link_num` 统计的是**已经建立（bit7=1）的 GMSL link 数**，不是 CSI lane 数。
* CSI-2 侧每路固定 **4 lane**（设备树 `num_lanes = "4"`），四路各自独立成一个 4-lane CSI-2 输出（表注释 `CSI output is 2x4`、`Set Lane Mapping for 4-lane port A`，【代码依据】`sgx-yuv-gmsl2-tbls.h:102-111`）。

---

## 1.5 I2C 透传与地址映射（本板最实用的一节）

### 1.5.1 三层地址

| 层 | 表示 | 本板值 | 依据 |
| --- | --- | --- | --- |
| Linux i2c（7bit） | `i2c-2` 上的设备 | 解串器 **0x29**；串行器 **0x42 / 0x44 / 0x46 / 0x48** | 【实测】`/sys/bus/i2c/devices/2-0029/name = max96712_1`；i2ctransfer 可访问 0x42/0x44/0x46/0x48 |
| 驱动内部（8bit） | `max96712_write_reg_Dser(..., slaveAddr, ...)` | `0x29<<1 = 0x52`（`MAX96712_ADDR`）、`0x42<<1 = 0x84` … | 【代码依据】`sgx-yuv-gmsl2-tbls.h:28` 注释 + `sgx-yuv-gmsl2.c:381` `source = 0x84 + (driver_data%4)*4`；`max96712.c:50` `i2c_client->addr = slaveAddr / 2` |
| 传感器侧（8bit） | 表格里的 `SENSOR_ADDR` 占位符 | `source = priv->ser_addr << 1` | 【代码依据】`sgx-yuv-gmsl2.c:376-379` |

对应关系表（**建议直接背下来**）：

| 链路 | 串行器 7bit | 串行器 8bit（驱动/表格） | 设备树节点 | i2c 客户端名 |
| --- | --- | --- | --- | --- |
| A（port 0） | `0x42` | `0x84` | `cam_0@1a` | `sgx-yuv-gmsl2-0` |
| B（port 1） | `0x44` | `0x88` | `cam_1@1b` | `sgx-yuv-gmsl2-1` |
| C（port 2） | `0x46` | `0x8C` | `cam_2@1c` | `sgx-yuv-gmsl2-2` |
| D（port 3） | `0x48` | `0x90` | `cam_3@1d` | `sgx-yuv-gmsl2-3` |

> `0x84` 恰好等于 `0x42<<1`，这不是巧合：驱动把所有"source 地址"统一按 8bit 记账，函数入口再 `>>1`。

### 1.5.2 Passthrough 机制：为什么必须"先选链路"

4 颗串行器上电时**地址相同**（代码用 8bit `0x80` 访问它们，【推断】对应 7bit `0x40` = 该类器件的默认地址）。要让它们各自可寻址，必须：

1. **先按位打开某一路的 I2C 通道**：写解串器 `0x0006`（【代码依据】`sgx-yuv-gmsl2.c:301`、`331`；表内注释 `// Enable all 4 Links in GMSL2 mode` 见 `sgx-yuv-gmsl2-tbls.h:49`）；
2. **通过该通道访问串行器**（此时用它的默认地址 `0x80`）：写 MFP/流水线寄存器（`0x02BE/0x02D6/0x0318/0x02D3` 等）；
3. **改写它的地址寄存器**：`max96712_write_reg_Dser(..., MAX96717_ADDR /*0x80*/, 0x0000, 0x84 + cam_id*4)`（【代码依据】`sgx-yuv-gmsl2.c:321`）→【推断】`0x0000` 是串行器的设备地址寄存器（DEV_ADDR），写完之后这颗串行器就固定在 `0x42/0x44/0x46/0x48` 上；
4. 之后（含 `i2ctransfer` 脚本）就可以**直接按 0x42/0x44/0x46/0x48 访问**对应串行器。

```
        SoC(i2c-2)                MAX96712(0x29)               MAX96717 ×4               sensor
            │                           │                          │                       │
            │  w 0x0006 = 1<<cam_id     │                          │                       │
            ├──────────────────────────►│  打开 link N 的 I2C 通道  │                       │
            │  w 0x80 0x0000 = 0x84+4N  │                          │                       │
            ├──────────────────────────►├─────────────────────────►│ 改本机地址 → 0x42+2N   │
            │  w/r 0x42+2N …            │                          │                       │
            ├──────────────────────────►├─────────────────────────►│ 后续都走固定地址        │
            │                           │                          │                       │
            │  w source=ser_addr<<1     │                          │                       │
            ├──────────────────────────►├─────────────────────────►├──────────────────────►│ sensor 寄存器
```

### 1.5.3 `i2ctransfer` 实操（可直接复制）

> 前提：`i2c-tools` 已装（本镜像有 `i2ctransfer`）；`-y` 跳过交互确认，`-f` = force（对已被驱动占用的地址强制访问）。

```bash
# 0) 看 bus 上有什么设备
i2cdetect -y -r 2

# 1) 读解串器（MAX96712）16bit 寄存器：link A/B/C/D 状态
sudo i2ctransfer -y -f 2 w2@0x29 0x00 0x1A r1     # → 本板实测 0xda（port0）
sudo i2ctransfer -y -f 2 w2@0x29 0x00 0x0A r1     # → 0xc8（port1）
sudo i2ctransfer -y -f 2 w2@0x29 0x00 0x0B r1     # → 0xc8（port2）
sudo i2ctransfer -y -f 2 w2@0x29 0x00 0x0C r1     # → 0xc8（port3）

# 2) 读/写解串器的链路使能寄存器
sudo i2ctransfer -y -f 2 w2@0x29 0x00 0x06 r1
sudo i2ctransfer -y -f 2 w3@0x29 0x00 0x06 0x0f    # 打开全部 4 路（示例）

# 3) 通过 passthrough 访问串行器（此处 port0 = 0x42）：
sudo i2ctransfer -y -f 2 w2@0x42 0x02 0xD3 r1      # 读 MFP7 配置 → 本板实测 0x18
sudo i2ctransfer -y -f 2 w3@0x42 0x02 0xD3 0x85    # 写（研华 fsync 脚本用的值，见 §1.7）

# 4) 四路串行器的 MFP7 一次看全（本板实测均为 0x18）
for a in 0x42 0x44 0x46 0x48; do
  echo -n "$a 0x02D3="; sudo i2ctransfer -y -f 2 w2@$a 0x02 0xD3 r1
done
```

**注意**：

* 解串器与串行器的寄存器地址都是 **16bit**（驱动 `reg_bits = 16`，见 `max96712.c:246-250`），因此读命令写成 `w2@addr 高字节 低字节 r1`。
* 从**串行器**读/写的前提是**该链路已建立且 I2C 通道已打开**；链路没锁上时对 0x42 的访问会失败（NACK/timeout）。
* 【实测】四路 MFP7 都是 `0x18` 且四路取流 30/30 帧、30.00 fps、0 错误——**这是本板"正常状态"的基线**。

---

## 1.6 CSI-2 侧：lane、虚拟通道、格式与设备树

### 1.6.1 四路 CSI-2 的关键参数（设备树）

【代码依据】`live.dts`：`max96712_1@29`(5513-5517) + `cam_0@1a`…`cam_3@1d`(5519 起，每路约 220 行)。

| 项 | 值 | 位置 |
| --- | --- | --- |
| i2c 控制器 | `i2c@3180000`（**i2c-2**），`clock-frequency = 0x61a80`（400 kHz） | `live.dts:5493-5511` |
| 解串器节点 | `max96712_1@29`，`compatible = "nvidia,max96712_1"`，`reg = <0x29>` | `live.dts:5513-5517` |
| 相机节点 ×4 | `cam_0@1a` / `cam_1@1b` / `cam_2@1c` / `cam_3@1d` | `live.dts:5519 / 5800 / 6081 / 6386` 起 |
| `compatible` | `sensing,sgx-yuv-gmsl2-0 … -3` | `live.dts:5528 / 5809 / 6090 / 6395` |
| `sensor_model` | `"sgx-yuv-gmsl2"` | `live.dts:5520` 等 |
| `der_id` | `<0x01>`（四路相同） | `live.dts:5524 / 5805 / 6086 / 6391` |
| `reg`（→ 驱动 `ser_addr`） | `0x1a / 0x1b / 0x1c / 0x1d` | `live.dts:5533` 等 |
| `def_addr` | `0x34` | `live.dts:5529` |
| `reset-gpios` | `<0xf0 0xa0 0x00>` | `live.dts:5527` |
| `mclk` | `"extperiph1"`，`mclk_khz = "24000"` | `live.dts:5525 / 5541` |
| `devnode` | `"video0"` … `"video3"` | `live.dts:5522` 等 |
| **vc_id** | **`0 / 1 / 2 / 3`**（每路一个） | `live.dts:5563 / 5844 / 6125 / 6430` |
| `num_lanes` | `"4"`（每路 4 lane） | `live.dts:5547` 等 |
| `mode_type` / `pixel_phase` | `"yuv"` / `"uyvy"` | `live.dts:5550-5551` |
| `csi_pixel_bit_depth` / `dynamic_pixel_bit_depth` | `"16"` / `"16"` | `live.dts:5567 / 5559` |
| `embedded_metadata_height` | `"0"`（无嵌入行） | `live.dts:5570` |
| `discontinuous_clk` | `"no"` | `live.dts:5538` |
| `cil_settletime` | `"0"` | `live.dts:5554` |

【实测】对应的设备节点与 subdev：

```
/dev/video0  ←  vi-output, sgx-yuv-gmsl2 2-001a
/dev/video1  ←  vi-output, sgx-yuv-gmsl2 2-001b
/dev/video2  ←  vi-output, sgx-yuv-gmsl2 2-001c
/dev/video3  ←  vi-output, sgx-yuv-gmsl2 2-001d
```

### 1.6.2 模式索引与分辨率（表）

【代码依据】厂商 `init.sh` 的数组与驱动 `mode_table[]`：

| `sensor_mode` | 分辨率（厂商 `init.sh` 数组） | `mode_table[]` 指向 | 传感器寄存器表是否为空 |
| --- | --- | --- | --- |
| 0 | 1920×1080 | `sensor_3840x2160_crop_30fps` | **空表**（只有 `TABLE_END`） |
| 1 | 1920×1536 | 同上 | 空 |
| 2 | 2880×1860 | 同上 | 空 |
| 3 | 3840×2160 | 同上 | 空 |
| 4 | 1280×720 | 同上 | 空 |
| 5 | 3840×2160-ISX028 | `isx028c_ES3_/ES2_3840x2160_crop_30fps`（由读 `0x8395` 判定用哪张） | 非空（真正写 sensor） |
| 6 | — | 【待验证】 | 【待验证】 |

> ⚠️ **索引→分辨率的权威来源是设备树的 `modeX` 节点顺序**，不是厂商 `init.sh` 里的数组：`use_sensor_mode_id = "true"`（`live.dts:5531`）让 ctrl 按 **索引**取值，而索引最终落到 `s_data->mode_prop_idx` → `sensor_modes[]`（由 DT 的 `modeX` 节点顺序生成，【代码依据】`sgx-yuv-gmsl2.c:633-634`）。
> - 索引 **0 = 1920×1080、1 = 1920×1536 两者一致**（DT 节点 `mode0`/`mode1` 与厂商数组相同，且**本板实测索引 0/1 的行为与之吻合**）；
> - 索引 **2~6 的对应关系【待验证】**（DT 节点顺序与厂商数组未逐一对齐；实测 `v4l2-ctl --list-ctrls` 显示 `sensor_mode` 为 `min=0 max=6`，即索引 6 也存在，含义未确认）。
> 用之前请先 `v4l2-ctl --list-ctrls` 看范围，再逐个试。

【代码依据】`sgx-yuv-gmsl2-tbls.h:122-124`：

```c
static struct index_reg_8 sensor_3840x2160_crop_30fps[] = {
	{0x00, TABLE_END, 0x00},
};
```

**这张"空表"是本章最重要的硬件事实之一**：选 0~4 号模式时，驱动**不会**给 sensor 写任何寄存器（`i2c_write_table(priv, mode_table[idx])` 立刻遇到 `TABLE_END` 返回）。因此：

* sensor 一直按它上电后的固定格式输出；
* 所谓"改分辨率"只是**改变了 VI/NVCSI 侧期望的裁剪与时序参数**（来自设备树各 `modeX`）；
* **模式能不能出图，取决于该 `modeX` 的参数是否与真实码流自洽**（见下）。

### 1.6.3 一个 mode 长什么样（mode0 vs mode1 对照）

【代码依据】`live.dts:5537-5574`(mode0) 与 `5682-5719`(mode1)：

| 参数 | mode0（1920×1080） | mode1（1920×1536） |
| --- | --- | --- |
| `active_w` / `active_h` | 1920 / **1080** | 1920 / **1536** |
| **`pix_clk_hz`** | **150000000** | **200000000** |
| `serdes_pix_clk_hz` | 350000000 | 350000000 |
| `line_length` | 2200 | 2200 |
| `num_lanes` | 4 | 4 |
| `default_exp_time` / `max_exp_time` | **33333 / 33333** | **22000 / 22000** |
| `min_exp_time` | 59 | 28 |
| `max_framerate` / `default_framerate` | 30000000 | 30000000 |

> `33333 µs` 恰好等于 30 fps 的**整帧周期**（33.333 ms）——【推断】这在某些 sensor 上属于"曝光吃满一帧"的边界配置。**是否就是 mode0 失败的原因，必须靠实验确认。**

### 1.6.4 本板实测：只有 mode1 能出图

| 操作 | 结果 |
| --- | --- |
| `v4l2-ctl -d /dev/video0 --set-ctrl bypass_mode=0,sensor_mode=1` + `--set-fmt-video=width=1920,height=1536,pixelformat=UYVY` + `--stream-mmap --stream-count=30` | **30 帧全部拿到，30.00 fps，0 个错误标记**【实测】 |
| 四路同上（各 30/30/100 帧） | **30/30/100 全部拿到，四路 30.00 fps，dmesg `corr_err/uncorr_err = 0`**【实测】 |
| `sensor_mode=0` + `width=1920,height=1080` | **取流挂住（`v4l2-ctl` 超时被杀，拿不到帧）**；此前一次尝试还观察到 `corr_err: discarding frame 0, flags: 0, err_data 64`【实测】 |
| 落盘校验 | `/tmp/frame0.yuv = 5898240 字节 = 1920×1536×2`（正好一帧 UYVY）【实测】 |

**根因【待验证】**，候选（按可疑度）：

1. mode0 的 `pix_clk_hz = 150 MHz` 与真实码流不符（真实码流不随模式变化，因为 sensor 表是空的）；
2. mode0 的 `default_exp_time = max_exp_time = 33333 µs` 等于整帧周期；
3. mode0 的裁剪高度 1080 与 4 lane/2200 `line_length` 组合在 VI 侧像素率不足（`150 MHz / 2200 ≈ 68 kHz` 行率，对应 2160 行仅 ≈ 31.5 fps，余量极小）。

> 实验方法（不需要重刷）：DTB 由 rootfs 提供（`/boot/dtb/kernel_…dtb`，见 `extlinux.conf` 的 `FDT` 行），可只改 `mode0` 的单个参数、换 DTB、重启，再用同一套 30 帧测试判定。**这是第 1 章留给后续章节的明确待办。**

---

## 1.7 帧同步/触发（fsync）的硬件原理

### 1.7.1 为什么需要 fsync

多相机系统常常要求**四路曝光起点严格对齐**（否则做多目拼接/测量时会抖）。做法是让**同一个同步信号**分发到所有串行器/sensor：

| 信号 | 芯片 | 角色 | 【推断】说明 |
| --- | --- | --- | --- |
| **MFP2** | MAX96712（解串器） | **帧同步输出**（脚本首行打印 `96712 MFP2_framesync setting`） | 解串器在收到外部触发（本板是 PWM）后，通过 MFP2 向所有 link 广播同步脉冲 |
| **MFP7** | MAX96717（串行器） | 被脚本改写（`0x02D3`），用于把同步/触发信号接入 sensor | 脚本注释写 `96717-mfp7 settings` |
| 外部触发 | 载板 PWM（`pwmchip2/pwm0`） | 30 Hz 方波 | 脚本把周期设成 `1e9/30` ns、占空比 10% 并 enable |

### 1.7.2 脚本原文（【脚本原文】，含义标【推断】）

来源：`remote-src/camera_fsync_set.sh`（研华 SENSING 包内同名文件）。

```bash
I2C_BUS=2 ; DESER_ADDR="0x29" ; TARGET_FPS=30

# ---- A. 解串器：帧同步相关（MFP2）----
i2ctransfer -f -y 2 w3@0x29 0x04 0xa0 0x08
i2ctransfer -f -y 2 w3@0x29 0x04 0xaf 0x9f ; sleep 0.2
i2ctransfer -f -y 2 w3@0x29 0x03 0x06 0x83
i2ctransfer -f -y 2 w3@0x29 0x03 0x07 0xa7
i2ctransfer -f -y 2 w3@0x29 0x03 0x3d 0xa7
i2ctransfer -f -y 2 w3@0x29 0x03 0x74 0xa7
i2ctransfer -f -y 2 w3@0x29 0x03 0xaa 0xa7 ; sleep 0.2

# ---- B. 四颗串行器：MFP7（0x02D3）----
for a in 0x42 0x44 0x46 0x48 ; do
  i2ctransfer -f -y 2 w3@$a 0x02 0xd3 0x85
  i2ctransfer -f -y 2 w3@$a 0x00 0x10 0x21 ; sleep 0.2
  i2ctransfer -f -y 2 w3@$a 0x02 0xd3 0x84
  i2ctransfer -f -y 2 w3@$a 0x02 0xd5 0x07 ; sleep 0.2
done

# ---- C. 外部触发：PWM 30 Hz ----
PWM_PERIOD=$(( 1000000000 / 30 ))      # 33333333 ns
PWM_DUTY=$(( PWM_PERIOD / 10 ))        # 10%
echo 0 > /sys/class/pwm/pwmchip2/export                 # 若尚未 export
echo 0 > /sys/class/pwm/pwmchip2/pwm0/enable
echo $PWM_PERIOD > /sys/class/pwm/pwmchip2/pwm0/period
echo $PWM_DUTY   > /sys/class/pwm/pwmchip2/pwm0/duty_cycle
echo 1 > /sys/class/pwm/pwmchip2/pwm0/enable
```

### 1.7.3 寄存器清单与解释（严格区分来源）

| 目标 | 寄存器 | 写入值 | 来源 | 含义 |
| --- | --- | --- | --- | --- |
| 解串器 | `0x04A0` | `0x08` | 【脚本原文】 | 【推断】帧同步/MFP2 功能使能位 |
| 解串器 | `0x04AF` | `0x9F` | 【脚本原文】 | 【推断】帧同步参数（周期/脉冲宽度相关） |
| 解串器 | `0x0306` | `0x83` | 【脚本原文】 | 【推断】GPIO/MFP 配置（某路/某组） |
| 解串器 | `0x0307` | `0xA7` | 【脚本原文】 | 【推断】同上（相邻寄存器） |
| 解串器 | `0x033D` | `0xA7` | 【脚本原文】 | 【推断】下一组链路的同功能寄存器 |
| 解串器 | `0x0374` | `0xA7` | 【脚本原文】 | 【推断】再下一组 |
| 解串器 | `0x03AA` | `0xA7` | 【脚本原文】 | 【推断】再下一组 |
| 串行器 ×4 | `0x02D3` | `0x85` → 后改 `0x84` | 【脚本原文】 | 【推断】MFP7 复用功能选择（先切成"同步输入"，再确认） |
| 串行器 ×4 | `0x0010` | `0x21` | 【脚本原文】 | 【推断】寄存器表页/功能切换 |
| 串行器 ×4 | `0x02D5` | `0x07` | 【脚本原文】 | 【推断】MFP7 相关参数（极性/延时） |
| 权限 | `0x02D3` 正常值 | **`0x18`** | 【实测】 | 未执行 fsync 时四路都是 `0x18` |

> **`0x0306 / 0x033D / 0x0374 / 0x03AA` 的地址间隔是 `0x37 / 0x37 / 0x36`，看起来是"每路一组 GPIO/MFP 寄存器块"，但步长不整齐 → 【待验证】。** 不要据此推断寄存器分组规则。

### 1.7.4 本板实测与结论

| 观测 | 结论 |
| --- | --- |
| 未执行 fsync 时四路 `0x02D3 = 0x18`，四路取流 30/30/100 帧、30.00 fps、0 错误 | **`0x18` 是"能正常出图"的状态**【实测】 |
| 上一轮排查中执行 `camera_fsync_set.sh`（即把 `0x02D3` 写成 `0x84/0x85` 并开 PWM）后，**相机停流** | 【实测】+【推断】fsync 与当前模组/接线不兼容（例如同步信号极性、PWM 与 sensor 期望的电平不一致） |
| 因此当前镜像**不在开机流程里执行** `camera_fsync_set.sh`（研华 `init.sh` 末尾会调用它） | 【实测】现状：镜像里没有自动执行 fsync 的路径 |

**给后续章节的待办**：若产品确实需要 4 路严格同步，需要：

1. 确认载板 PWM 输出是否真的接到解串器 MFP2（`pwmchip2` 到 MFP2 的物理走线）【待验证】；
2. 确认 sensor 侧是否期望 MFP7 作为同步输入、电平/极性是否匹配【待验证】；
3. 逐寄存器二分定位"哪一条写下去导致停流"（可用 `i2ctransfer` 在线试）【待验证】。

---

## 1.8 本板拓扑小结

```
                       ┌─────────────────────────── Jetson Orin NX (P3767) ───────────────────────────┐
                       │  NVCSI / VI  (4 × CSI-2, 每路 4 lane, vc_id 0/1/2/3, UYVY 16bit)              │
                       └────────────▲───────────────▲───────────────▲───────────────▲─────────────────┘
                                    │ CSI-A         │ CSI-B         │ CSI-C         │ CSI-D
                       ┌────────────┴───────────────┴───────────────┴───────────────┴─────────────────┐
                       │        MAX96712 解串器（载板）   i2c-2 @0x29 (8bit 0x52)                       │
                       │        · 4 路 GMSL2 链路状态：0x001A/0x000A/0x000B/0x000C                        │
                       │        · 链路使能/透传选择：0x0006（bit0~3 = link A~D）                          │
                       │        · 外部触发输入 MFP2（由 pwmchip2 驱动）【推断】                            │
                       └───┬────────────────┬────────────────┬────────────────┬────────────────────────┘
                   同轴 A  │          同轴 B │          同轴 C │          同轴 D │
                       ┌───▼────┐      ┌────▼───┐       ┌────▼───┐      ┌────▼───┐
                       │MAX96717│      │MAX96717│       │MAX96717│      │MAX96717│   （模组内）
                       │ 0x42   │      │ 0x44   │       │ 0x46   │      │ 0x48   │  ← 地址由 §1.5.2 流程分配
                       │ MFP7   │      │ MFP7   │       │ MFP7   │      │ MFP7   │
                       └───┬────┘      └───┬────┘       └───┬────┘      └───┬────┘
                       ┌───▼────┐      ┌───▼────┐       ┌───▼────┐      ┌───▼────┐
                       │ sensor │      │ sensor │       │ sensor │      │ sensor │   sensor 8bit 地址 = ser_addr<<1
                       └────────┘      └────────┘       └────────┘      └────────┘   （0x34/0x36/0x38/0x3a）
```

| 层次 | 数量 | 关键标识 | 依据 |
| --- | --- | --- | --- |
| 相机模组 | 4 | SGX-YUV-GMSL2，串行器 MAX96717 | 设备树 `compatible`/`sensor_model`；【实测】`been detected` ×4 |
| GMSL 链路 | 4 | GMSL2 / 3G（`GMSLMODE=2`），实测 link `0xDA/0xC8/0xC8/0xC8` | 【实测】+【代码依据】 |
| 解串器 | 1 | `2-0029`，`compatible = "nvidia,max96712_1"`，`der_id = 1` | 【实测】+【代码依据】 |
| CSI-2 输出 | 4 | 每路 4 lane，`vc_id = 0/1/2/3` | 【代码依据】`live.dts` |
| 设备节点 | 4 | `/dev/video0..3` ← `vi-output, sgx-yuv-gmsl2 2-001a..d` | 【实测】 |
| 模块参数 | — | `GMSLMODE_1=2,2,2,2`；`GMSLMODE` 只读，加载时生效 | 【实测】+【代码依据】 |

**软件侧的"三件套"**（后续章节展开）：

1. `max96712.ko`：解串器驱动，提供 `max96712_{read,write}_reg_Dser()` 等导出符号，并把 `global_priv[der_id]` 填好；
2. `sgx-yuv-gmsl2.ko`：相机驱动，负责上电、链路检查、GMSLMODE 生效、模式切换、（可选）fsync；
3. `tegra-camera.ko` / `nvhost-nvcsi-t194.ko` / `capture-ivc.ko`：NVIDIA 相机框架（本板必须用研华预编译版本，原因见后续章节）。

---

## 1.9 依据清单（本章引用的文件与行号）

### 驱动源码

| 文件 | 行号 | 本章用到的内容 |
| --- | --- | --- |
| `remote-src/source/nvidia-oot/drivers/media/i2c/MIC-MAX96712/sgx-yuv-gmsl2.c` | 198-215 | `struct sensor` 成员（`der_id`、`ser_addr`、`link_num[4]`、`GMSLMODE[4]`） |
| 同上 | 218-221 | GMSL2/GMSL1 链路状态寄存器表 |
| 同上 | 230-240 | `GMSLMODE_0..3` 定义与 `module_param_array`（`S_IRUGO`） |
| 同上 | 242-285 | `sensor_read_reg` / `sensor_write_reg`（经 `i2c_client->addr` 切地址） |
| 同上 | **287-355** | **`dser_link_check()` 全流程（§1.3）** |
| 同上 | 357-401 | `i2c_write_table()`：`SENSOR_ADDR`/`MAX96717_ADDR`/`MAX96712_ADDR` → source 地址映射（§1.5） |
| 同上 | **598-626** | **`max96712_set_lane_rate()`：link_num 统计、rate 公式、0x041B/0x903…（§1.4）** |
| 同上 | 628-684 | `sensor_set_mode()`：`lane_rate` 计算(670)、ISX028 分支与 `ser_addr<<1`(650-664)（§1.4/§1.6） |
| 同上 | **744-796** | **`max96712_init_mode()`：GMSLMODE 逐路生效（0x0010/0x0011、0x041A/0x0415/0x0418 vs 0x0006）（§1.2）** |
| 同上 | 832-833 | `der_init_flag[4]`、`probe_count` |
| 同上 | 834-921 | `sensor_probe`：`der_id`(859)、`ser_addr`(873)、**GMSLMODE 拷贝(895)**、`der_init_flag` 流程(901-921)、`dser_link_check` 调用(917) |
| 同上 | 955-993 | `sensor_id[]`（`sgx-yuv-gmsl2-0..15`）、`sensor_of_match[]`（36-65）、`i2c_driver` |
| `remote-src/source/nvidia-oot/drivers/media/i2c/MIC-MAX96712/max96712.c` | 31 | `global_priv[4]` |
| 同上 | 36-134 | 三个导出函数（`slaveAddr/2` 的地址换算、`global_priv[device_id]==NULL` 分支） |
| 同上 | 200-244 | `max96712_debugfs_init()`：**219 行登记 `global_priv[]`** |
| 同上 | 246-250 | `regmap_config`：`reg_bits = 16`（24bit 地址形式） |
| 同上 | 252-314 | `max96712_probe()`（**不做任何 I2C 读写**）、`max96712_id[]`（`max96712_0..3`） |
| `remote-src/source/nvidia-oot/drivers/media/i2c/MIC-MAX96712/sgx-yuv-gmsl2-tbls.h` | 23-37 | `TABLE_WAIT_MS/TABLE_END`、`SENSOR_ADDR/MAX96712_ADDR/MAX96717_ADDR`、`struct index_reg_8` |
| 同上 | 47-120 | `max96712_gmsl2_6G_tbl`（含 0x0415/0x0418/0x041B/0x041E=0x37 的厂商注释） |
| 同上 | **122-124** | **`sensor_3840x2160_crop_30fps[]` 为空表（§1.6.2 关键依据）** |
| 同上 | 127-149+ | `max96712_gmsl1_tbl`（注释 `Set 3Gbps link Rx rate for GMSL1` 等） |
| 同上 | 438-464 | `enum` 与 `mode_table[]`（0~4 号全部指向空表；5/6 指向 ISX028 表） |
| 同上 | 466-477 | `sensor_frmfmt[]`（分辨率 ↔ 模式枚举） |

### 设备树

| 文件 | 行号 | 内容 |
| --- | --- | --- |
| `remote-src/live.dts` | 5493-5511 | `i2c@3180000`（i2c-2，400 kHz） |
| 同上 | 5513-5517 | `max96712_1@29`（`compatible = "nvidia,max96712_1"`, `reg = <0x29>`） |
| 同上 | 5519-5535 | `cam_0@1a`（`der_id`、`def_addr`、`reset-gpios`、`mclk`、`devnode`） |
| 同上 | 5537-5574 | `mode0`（1920×1080、`pix_clk_hz=150 MHz`、`exp=33333`） |
| 同上 | 5682-5719 | `mode1`（1920×1536、`pix_clk_hz=200 MHz`、`exp=22000`） |
| 同上 | 5800-5809 / 6081-6090 / 6386-6395 | `cam_1/cam_2/cam_3` 节点头（`vc_id` 分别 1/2/3） |
| 同上 | 5528 / 5809 / 6090 / 6395 | `compatible = "sensing,sgx-yuv-gmsl2-0..3"` |

### 脚本与配置

| 文件 | 说明 |
| --- | --- |
| `remote-src/camera_fsync_set.sh`（全文 80 行） | fsync 脚本原文（§1.7 全部寄存器） |
| 板上 `/etc/modprobe.d/mic712-camera.conf` | `options sgx-yuv-gmsl2 GMSLMODE_1=2,2,2,2`（【实测】） |

### 本会话实测记录（板上）

| 观测 | 值 |
| --- | --- |
| 四路 link 状态 | `0xda` / `0xc8` / `0xc8` / `0xc8` |
| 四路串行器 MFP7（`0x02D3`） | `0x18` ×4 |
| `link_num` / `lane_rate` | `4` / `0xf6` |
| 驱动版本串 | `sgx-yuv-gmsl2-0_v2.0.6` |
| 四路取流 | 各 30/30/100 帧，30.00 fps，`corr_err/uncorr_err = 0` |
| 可用模式 | `sensor_mode=1`（1920×1536 UYVY）；`sensor_mode=0` 挂住 |
| i2c 拓扑 | `i2c-2`；解串器 `2-0029`(name `max96712_1`)；串行器 `0x42/0x44/0x46/0x48` |
| 相机 subdev | `vi-output, sgx-yuv-gmsl2 2-001a..2-001d` → `/dev/video0..3` |

---

## 1.10 本章遗留的【待验证】清单

| # | 问题 | 建议确认方式 |
| --- | --- | --- |
| 1 | 链路状态寄存器 `0x001A/0x000A/0x000B/0x000C` 的位定义（尤其 `0x8`、`0x80`、`0x90`） | 查 MAX96712 datasheet；或用"拔/插同轴"对照读值 |
| 2 | `0xDA`（port0）与 `0xC8`（其余三路）低 4 位差异的含义 | 同 1，或换线/换模组对照 |
| 3 | `GMSLMODE = 1`（研华注：GMSL2/6G）在代码里只体现"不动 0x0010/0x0011 对应位"的确切语义 | 读 `0x0010/0x0011` 在各档位下的实际值 |
| 4 | `mode0`（1920×1080）失败根因（`pix_clk_hz`？曝光=整帧？裁剪高度？） | 改 DTB 单参数 + 重启 + 30 帧测试（见 §1.6.4） |
| 5 | `0x0415/0x0418/0x041B/0x041E` 本板实际生效值与链路速率 | 在线读寄存器；对照模组手册 |
| 6 | fsync 导致停流的具体寄存器与原因；PWM→MFP2 的物理走线 | 逐寄存器二分；查载板原理图 |
| 7 | 串行器默认地址（代码里的 8bit `0x80`）是否即 7bit `0x40` | 未配置链路前 `i2cdetect` 观察 |
| 8 | i2c 客户端名为何是 `sgx-yuv-gmsl2-0`（而非带厂商前缀的 compatible） | 查 i2c/of 设备命名规则并实测验证 |
| 9 | PoC 供电与 reset-gpios 的具体硬件路径 | 载板原理图 |

---

*（第 1 章完。下一章：驱动与模块加载顺序、max96712/sgx-yuv-gmsl2 的 probe 流程与已知坑。）*
