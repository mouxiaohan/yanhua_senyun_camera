# yanhua_senyun_camera

MIC-712AR（Advantech 载板 + NVIDIA Jetson Orin NX）搭载 **森云 Sensing SGX-YUV-GMSL2** 四路 GMSL2 相机的
驱动源码、参考资料与开发/排障文档。

| 项目 | 值 |
|---|---|
| 平台 | Advantech MIC-712AR 载板 + Jetson Orin NX（P3767 + P3768） |
| 软件基线 | L4T R36.4.3 / JetPack 6.2，内核 `5.15.148-tegra` |
| 相机 | SGX-YUV-GMSL2 ×4（GMSL2 3G） |
| 拓扑 | 解串器 **MAX96712 @ i2c-2 / 0x29** ← 4× GMSL2 同轴 ← 串行器 **MAX96717 @ 0x42 / 0x44 / 0x46 / 0x48** |
| 出图 | `/dev/video0..3`，推荐 `sensor_mode=1` + 1920×1536 UYVY @30fps |
| 传感器原生输出 | 3840×2160@30（各"分辨率"由设备树 `modeX` 裁剪/时序决定） |

---

## 文档（建议按顺序阅读）

| # | 文档 | 内容概要 | 适合 |
|---|---|---|---|
| 0 | [00-索引与术语速查.md](00-索引与术语速查.md) | 阅读顺序、系统拓扑、术语表、**10 条核心结论**、快速开始命令 | 所有人（先读这个） |
| 1 | [01-GMSL2-SerDes原理与硬件.md](01-GMSL2-SerDes原理与硬件.md) | GMSL2 链路原理、串行器/解串器分工、链路锁定判据与寄存器、I2C 透传三层地址、CSI-2/VC、帧同步硬件原理 | 驱动/BSP |
| 2 | [02-软件栈与加载初始化出图全流程.md](02-软件栈与加载初始化出图全流程.md) | 模块清单与职责、加载机制（modules-load/modprobe.d/udev/符号依赖）、**为什么 max96712 必须先 probe**（含 panic 0x88 机理与反汇编证据）、probe 时序、出图前置条件、媒体图 | 驱动/BSP |
| 3 | [03-设备树与配置详解.md](03-设备树与配置详解.md) | overlay 与构建期合并、节点结构、`modeX` 26 个参数逐项详解、mode0/mode1 实测对比、`GMSLMODE_1`/`sensor_mode`/fsync 配置、配置影响矩阵 | 驱动/BSP |
| 4 | [04-从零写驱动与触发同步驱动.md](04-从零写驱动与触发同步驱动.md) | 拿到模组后的信息收集、解串器驱动模板、tegracam sensor 驱动模板、寄存器表写法、**触发/同步驱动**、验证阶梯、踩坑清单 | 驱动开发 |
| 5 | [05-排障手册与案例复盘.md](05-排障手册与案例复盘.md) | 症状→原因→检查→修复总表与逐条展开、调试工具箱、**三个真实故障复盘**、上线检查清单 | 现场/值班 |
| 6 | [06-系统完整拓扑图.md](06-系统完整拓扑图.md) | **完整拓扑图**：物理/协议总图（CPU→串解串器→模组）、I2C 控制通路三层地址、电源/时钟/同步、软件栈映射、取流全路径、速查表 | 所有人（先看图最快） |
| 7 | [07-读数据函数调用流程.md](07-读数据函数调用流程.md) | **读帧/读寄存器的完整函数调用链**：REQBUFS→QBUF→STREAMON→DQBUF 每一层函数（vb2/VI5 csi5/RTCPU/sensor）、时间线、可观测点与卡点定位 | 驱动/排障 |

文档约定：全文区分 **【实测】/【代码依据 file:line】/【DT live.dts:line】/【推断】/【待验证】**，
不确定项集中列在各章末尾，未编造寄存器位定义。

---

## 三分钟速查

**1) 加载顺序（最重要，否则开机会 panic 0x88）**

```
/etc/modprobe.d/mic712-camera.conf
    options  sgx-yuv-gmsl2 GMSLMODE_1=2,2,2,2     # _1 = der_id(来自 DT compatible)，2 = GMSL2/3G
    softdep  sgx-yuv-gmsl2 pre: max96712          # 保证解串器先加载并完成 probe
```

**2) 出图五步**

```bash
# ① 模块与绑定
modinfo max96712 | grep '^alias'          # 必须有 i2c:max96712_0.._3（研华预编译件只有 i2c:max96712 → 永不绑定）
ls /sys/bus/i2c/drivers/max96712/         # 必须有 2-0029
# ② 顺序（probe success 必须早于 sgx 的 tegracam 行）
sudo dmesg | grep -E 'max96712_probe: probe success|tegracam sensor driver' | head -6
# ③ NVCSI 时钟（否则 corr_err / 超时）
sudo sh -c 'echo 1 > /sys/kernel/debug/bpmp/debug/clk/nvcsi/mrq_rate_locked; \
            echo 214300000 > /sys/kernel/debug/bpmp/debug/clk/nvcsi/rate'
# ④ 取流
for p in 0 1 2 3; do
  sudo v4l2-ctl --set-ctrl bypass_mode=0,sensor_mode=1 -d /dev/video$p
  sudo v4l2-ctl -d /dev/video$p --set-fmt-video=width=1920,height=1536,pixelformat=UYVY \
    --stream-mmap --stream-count=30 --verbose 2>&1 | grep -c 'cap dqbuf'
done   # 期望 30 30 30 30
# ⑤ 判据
sudo dmesg | grep -c 'been detected'          # 4
sudo dmesg | grep -c "can't find DER adapt"   # 0
```

**3) 模块来源铁律**

| 模块 | 来源 |
|---|---|
| `max96712.ko`、`sgx-yuv-gmsl2.ko` | **必须自编译**（研华预编译件别名不匹配） |
| `tegra-camera.ko`、`nvhost-nvcsi-t194.ko`、`capture-ivc.ko` | **必须用研华预编译件**（无源码，自编译会 NULL deref） |

---

## 目录结构

```
.
├── 00-索引与术语速查.md                  # ← 文档入口
├── 01-GMSL2-SerDes原理与硬件.md
├── 02-软件栈与加载初始化出图全流程.md
├── 03-设备树与配置详解.md
├── 04-从零写驱动与触发同步驱动.md
├── 05-排障手册与案例复盘.md
├── 06-系统完整拓扑图.md                  # ← 完整拓扑图（CPU→解串器→串行器→模组）
├── 07-读数据函数调用流程.md              # ← 读帧/读寄存器的函数调用链
└── MIC-712AR_Orin_NX_JP6.2_Sensing_Universal_V1.0.0_source/
    ├── Readme.md / version.md            # 研华原始说明
    ├── source/                           # 相机驱动源码（max96712.c / sgx-yuv-gmsl2.c / overlay dts）
    ├── ko/ · ko_backup/ · dtb/           # 研华预编译件（对照参考，注意有两套不同 revision）
    ├── cam.sh / init.sh / clock_config.sh / camera_fsync_set.sh / acerobotics_camera.service
    └── boot/Image
```

> 注：研华 `ko/` 与 `ko_backup/` 里的 `max96712.ko` 是**不同 revision**：
> 旧版（别名仅 `i2c:max96712`，含 NULL 保护）与新版（别名 `_0.._3`，**丢掉了 NULL 保护 → 会 panic**）。
> 详细反汇编对照见文档 05 章 §5.4 案例 2。`source/` 目录为参考用途，构建请以主 SDK 跟踪树为准。

---

## 相关提交（主 SDK 仓库）

| commit | 说明 |
|---|---|
| `36c47474b` | max96712 NULL-deref panic 修复（恢复 NULL/越界保护）+ softdep 顺序保证 |
| `0ca1cb6ef` | 把上述修复镜像到 `vendor/kernel`（消除 source/ 漂移） |
| `515cca714` | `sgx-yuv-gmsl2` 改为源码编译 |
| `593764cc0` | 改用源码版 `max96712`（研华预编译件别名不匹配） |
