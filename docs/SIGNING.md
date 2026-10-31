# 代码签名说明（可选）

Typaste 本身不需要签名也能运行，但未签名的 exe 会触发 SmartScreen 警告。
本仓库提供两种签名方式，**都不会把证书提交进仓库**。

## 1. 准备证书

### 自签名（仅用于自测）

```powershell
$cert = New-SelfSignedCertificate -Type CodeSigningCert `
    -Subject "CN=YHuanheg" -CertStoreLocation Cert:\CurrentUser\My `
    -KeyUsage DigitalSignature -FriendlyName "Typaste dev"
$pwd = ConvertTo-SecureString -String "P@ssw0rd" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath typaste.pfx -Password $pwd
```

自签名证书只对同样信任该证书的机器有效；对公网用户仍会出现警告，
长期分发建议购买 OV/EV 代码签名证书。

### 商业证书

从 CA 取得 `.pfx` 与密码即可，后面的步骤完全相同。

## 2. 在 GitHub Actions 中启用

1. 把 pfx 转成 base64 并复制：

   ```powershell
   [Convert]::ToBase64String([IO.File]::ReadAllBytes("typaste.pfx"))
   ```

2. 仓库 → Settings → Secrets and variables → Actions，新增：
   - `CODESIGN_PFX_BASE64`：上一步的 base64 字符串
   - `CODESIGN_PASSWORD`：pfx 密码

3. 之后每次 `build.yml` 运行都会执行 `tools/sign.ps1`；
   未配置 secret 时该步骤自动跳过。

## 3. 本地签名

```powershell
$env:CODESIGN_PFX_BASE64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes("typaste.pfx"))
$env:CODESIGN_PASSWORD = "P@ssw0rd"
pwsh -File tools/sign.ps1 -File build-x64\Typaste.exe
```

脚本会调用最新版 Windows SDK 的 `signtool`，使用 SHA-256 + 时间戳
（`http://timestamp.digicert.com`），签名后自动 `verify /pa`。

## 4. 验证

```powershell
Get-AuthenticodeSignature build-x64\Typaste.exe | Format-List
```
