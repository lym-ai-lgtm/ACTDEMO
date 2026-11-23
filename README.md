# ACTDEMO

Unreal Engine 5.8 单机动作战斗 Demo，使用 C++、蓝图、GAS 和 Enhanced Input。

本仓库包含源码、配置、地图、蓝图、动画、模型、音频与特效。不是预编译发行包，需要安装UE及C++工具链后生成模块。

## 获取完整工程

安装 Git 与 Git LFS，执行：

```powershell
git lfs install
git clone https://github.com/lym-ai-lgtm/ACTDEMO.git
cd ACTDEMO
git lfs pull
```

不要只依赖网页 Download ZIP：LFS资产可能只得到指针文件。缺失资产时首先检查Git LFS下载结果和账户配额。

## 打开与运行（Windows）

1. 安装 Unreal Engine 5.8，以及兼容的 Visual Studio C++游戏开发工作负载和Windows SDK。
2. 右键 ACTDEMO.uproject，生成Visual Studio项目文件。
3. 打开生成的解决方案，选择 Development Editor / Win64，编译ACTDEMO。
4. 打开 ACTDEMO.uproject，等待首次着色器编译。
5. 打开 /Game/Demo/Maps/Lvl_MainMenu，点击运行，进入训练模式。
6. 点击增加假人或添加主动攻击敌人，将卡片拖到附近地面松开生成。Esc返回菜单。

项目描述已去除原开发环境的MCP/Toolset编辑器自动化插件。其专用校准工具目录Content/Demo/GAS/Katana/Editor未纳入运行工程；实际游戏资产保留。原工作区不受此导出调整影响。

## 功能

- 剑/太刀装备、地面轻重连段、轻重变招、武器切换攻击。
- 挑飞、空中追击、连击、下砸、气势强力技。
- 普通格挡、完美防御、气势恢复与破防、正背面伤害判断。
- 主动敌人周旋、普攻、跳斩与后撤。
- 锁定镜头、方向翻滚、无敌窗口、HUD冷却与训练拖放。

## 文档

- [功能复现手册](Docs/战斗系统制作手册.md)
- [开发历史记录](Docs/战斗系统制作手册_历史记录_2026-11-23.md)

按键和机制详见手册。第三方素材的权利属于各自权利人，本仓库公开不改变其原许可条件。作者确认本次上传素材允许公开再分发；未另行授予覆盖所有素材的统一开源许可证。

## 验证范围

原开发工程已通过Windows打包和用户演示测试；本仓库导出版清理了自动化插件依赖，仍需在干净机器上验证首次编译与运行。当前未完成多人同步测试。
