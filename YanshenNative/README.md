# YanshenNative — 眼神插件原生 C++ 实现

面向**原始 Delphi M2Server** 的原生注入插件，重新实现眼神插件 (yanshen2.0.7) 的功能。

## 架构

```
M2Server.exe (Delphi, 32位)
    │
    ├── 链接 libmysql.dll (代理)
    │     └── 代理加载 ys\yanshen2.0.7.dll
    │           └── 实际文件 = YanshenNative.dll (本插件)
    │                 ├── DllMain → 延迟初始化线程
    │                 │     ├── 读取 config.json / MyJson
    │                 │     └── Hook GetBagItemCount (0x007447C0)
    │                 ├── !!!! 隧道协议解析
    │                 ├── 41 个命令分发
    │                 └── 特征开关 (config.json 中文键)
    │
    └── PAS 脚本引擎
          └── GetBagItemCount('!!!!cmd:params$')
                └── 命中 Hook → 插件处理 → 返回结果
```

## 关键机制

### 1. 注入方式

原版机制：`libmysql.dll` 代理劫持 MySQL 调用，同时 `LoadLibrary("ys\\yanshen2.0.7.dll")` 加载插件。

本插件利用同一入口：把 `YanshenNative.dll` 改名为 `ys\yanshen2.0.7.dll` 即可自动加载。
`DLL_PROCESS_ATTACH` 后启动延迟初始化线程（等 5 秒让 M2Server 完成脱壳初始化）。

### 2. Hook 目标

- **函数**: `TPlayObject.GetBagItemCount` @ flat image `0x007447C0`
- **方式**: 5 字节相对跳转 (E9) inline hook + 可执行内存 stub
- **检测**: 模式扫描 Delphi prologue (`55 8B EC 83 C4`) 保底 RVA `0x003447C0`

### 3. 隧道协议

与 C# 版完全兼容：
```
!!!!集成函数,commandID,param1,param2,...,paramN$
!!!!commandID,param1,param2,...,paramN$   (legacy)
!!!!命令名 参数1:参数2:参数3:              (中文名)
!!!!分隔符^commandID^param1^param2^...$
itemName!!!!#ys,ys1,...,ys17$            (给元素物品)
itemName!!!!ys1|ys2|ys3|ys4|ys5|         (旧格式)
```

未命中的命令返回 `-1656`，脚本引擎回落宿主 GetBagItemCount。

## 目录结构

```
YanshenNative/
├── CMakeLists.txt       # CMake 构建配置
├── include/
│   ├── hook.h           # InlineHook + 函数查找
│   ├── tunnel.h         # !!!! 隧道协议
│   ├── config.h         # JSON 配置读取
│   └── commands.h       # 命令引擎
└── src/
    ├── dllmain.cpp      # DLL 入口 + Hook stub + 初始化
    ├── hook.cpp         # Hook 实现
    ├── tunnel.cpp       # 协议解析
    ├── config.cpp       # 配置管理
    └── commands.cpp     # 命令分发 + 特征开关
```

## 构建

```bash
# 32位 (M2Server 是 32位程序)
i686-w64-mingw32-gcc -shared -o bin/YanshenNative.dll \
    src/*.cpp -Iinclude -O2 -static -lstdc++ \
    -lkernel32 -luser32 -ladvapi32 -lole32 -lshlwapi

# 或用 CMake
cmake -B build -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++
cmake --build build
```

## 部署

1. 编译得到 `YanshenNative.dll`
2. 复制到 `E:\GOW\mud2.0\Mir200\Gs1\ys\YanshenNative.dll`
3. 双击 `ys\deploy.bat`（自动备份原版 + 改名部署）
4. 启动 M2Server.exe
5. 用 DebugView 查看 `[YanshenNative]` 日志确认加载成功

恢复原版：双击 `ys\restore.bat`

## 配置兼容

- `config.json` — 与 C# 版/原版完全一致（GBK 编码，中文键）
- `MyJson/` — 兼容 skills/roles/items/recycle/爆率 等

## 当前状态 / 待完成

- [x] 项目骨架（CMake + 32位编译）
- [x] DLL 入口 + 延迟初始化
- [x] Inline hook 基础设施
- [x] !!!! 隧道协议解析器
- [x] config.json 读取器（GBK）
- [x] 命令分发框架（41 命令注册）
- [ ] 元素系统完整实现 (17元素)
- [ ] 自定义伤害公式完整实现
- [ ] 宠物/宝宝系统
- [ ] 自动回收系统
- [ ] 技能补丁 (火墙/弹射/吸血等)
- [ ] 触发系统 (触发脚本/事件分发)
- [ ] MyJson 各配置解析
- [ ] 真机测试 (M2Server 加载验证)

## 已知风险

1. **M2Server 加壳** (Themida/WinLicense) — 脱壳延迟需要 5 秒初始化等待
2. **函数地址不匹配** — GetBagItemCount 地址基于 flat image 分析，若该版本 M2Server 布局不同需重新定位
3. **调用约定** — Delphi register 约定与 C++ 不同，hook stub 需要精确处理寄存器保存/恢复
4. **反作弊检测** — 部分 M2Server 版本检测内存修改，hook 可能被检测

## 与 C# 版的关系

本项目的功能逻辑参考 `LyoMir2\GameSvr\Plugins\` 下的 C# 实现（YanshenApi.cs, YanshenCommands.cs, YanshenTriggerDispatch.cs 等），是把同一套业务逻辑移植到原生 C++。