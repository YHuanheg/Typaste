# Typaste 文档索引

面向人类读者的入口。AI 行为约束见根目录 [CLAUDE.md](../CLAUDE.md)。

| 文档 | 内容 | 读者 |
| --- | --- | --- |
| [ARCHITECTURE.md](ARCHITECTURE.md) | 模块划分、为什么拆模块、LANG_NEUTRAL、C++17、DPI 基准布局、Typaste.ini 格式 | 接手开发者 |
| [IMPROVEMENTS.md](IMPROVEMENTS.md) | 从 1.0.5 到 1.4.0 的改进项与实测结论，含仍未做的建议 | 规划者 / 评审 |
| [SIGNING.md](SIGNING.md) | 证书准备、base64 / secrets 配置、本地与 CI 签名、验证方法 | 发布者 |
| [../README.md](../README.md) | 项目简介、功能、构建、下载 | 首次访问者 |

## 快速命令

```bat
build_x64.bat                                          :: 本地 x64 构建
tools\verify_lang.bat                                  :: 校验 4 语言 + LANG_NEUTRAL
python tools/gen_lang.py                               :: 重新生成 lang/*.rc
pwsh tools\sign.ps1 -File build-x64\Typaste.exe        :: 可选签名
```

## 运行时文件

| 文件 | 位置 | 说明 |
| --- | --- | --- |
| `Typaste.ini` | exe 同目录 | 便携设置；删除即回退注册表 |
| `HKCU\SOFTWARE\Katayama Hirofumi MZ\Typaste` | 注册表 | 非便携模式的设置 |
