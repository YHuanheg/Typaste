# sign.ps1 --- code-sign a built binary
#
# The certificate is provided through environment variables so that nothing
# secret ever has to be committed:
#
#   CODESIGN_PFX_BASE64 : base64 of the .pfx file
#   CODESIGN_PASSWORD   : the password of the .pfx
#
# Usage:
#   pwsh -File tools/sign.ps1 -File build-x64\Typaste.exe
#
# When CODESIGN_PFX_BASE64 is empty the script does nothing and returns 0,
# so that unsigned local builds keep working.
param(
    [Parameter(Mandatory = $true)][string]$File,
    [string]$TimestampUrl = 'http://timestamp.digicert.com'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $File)) {
    throw "File not found: $File"
}

$b64 = $env:CODESIGN_PFX_BASE64
if ([string]::IsNullOrWhiteSpace($b64)) {
    Write-Host "[sign] CODESIGN_PFX_BASE64 is not set; skipping the signature."
    exit 0
}

$password = $env:CODESIGN_PASSWORD
$pfxPath = Join-Path ([IO.Path]::GetTempPath()) ("typaste-codesign-" + [Guid]::NewGuid().ToString("N") + ".pfx")
[IO.File]::WriteAllBytes($pfxPath, [Convert]::FromBase64String($b64))

try {
    $signtool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $signtool) {
        $signtool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x86\signtool.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
    }
    if (-not $signtool) {
        throw "signtool.exe was not found. Install the Windows SDK signing tools."
    }

    Write-Host "[sign] Using $($signtool.FullName)"
    & $signtool.FullName sign /fd SHA256 /f $pfxPath /p $password /tr $TimestampUrl /td SHA256 $File
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed with exit code $LASTEXITCODE"
    }

    & $signtool.FullName verify /pa /v $File
    Write-Host "[sign] Signed: $File"
}
finally {
    if (Test-Path -LiteralPath $pfxPath) { Remove-Item -LiteralPath $pfxPath -Force }
}
