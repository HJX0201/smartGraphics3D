# Security Policy

## Supported version

安全修复仅面向最新的 `v0.1.x` Beta。更早的本地开发快照不受支持。

## Reporting

请不要在公开 Issue 中附加客户模型、`.sgdiag` 诊断包、本机路径、许可证密钥或其他敏感
数据。可先创建不含附件的安全问题说明，维护者会提供后续的私密传输方式。

报告应包含受影响版本、可复现步骤、预期/实际结果和影响范围。不要公开利用代码，直到
维护者完成确认和修复窗口协调。

本项目只处理本地 CAD 数据，不应以管理员身份运行。仅从项目 Releases 或自行验证的
源码构建获取可执行文件。

## Repository and release checks

仓库不得跟踪 SDK、工具链压缩包、构建产物、客户模型、诊断包或项目文件。提交前运行：

```powershell
python scripts/security/test_audit_sensitive_data.py
python scripts/security/audit_sensitive_data.py --require-noreply
```

发布前还必须扫描实际目录或 ZIP；扫描器只报告类别与文件位置，不打印疑似凭据内容：

```powershell
python scripts/security/audit_sensitive_data.py --release dist\smartGraphics3D-windows-x64
python scripts/security/audit_sensitive_data.py --release dist\smartGraphics3D-windows-x64.zip
```

如果历史中出现敏感信息，应先撤销或轮换凭据，再使用独立镜像清理历史；不要仅删除当前
分支中的文件。所有本地提交使用 GitHub `noreply` 邮箱。
