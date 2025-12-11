# ----------------------------------------------------
# 脚本配置区
# ----------------------------------------------------

$AmJsonSourcePath = "C:\Users\KP9\Desktop\AssetManifest.json" 
$ZipTool = "C:\Program Files\7-Zip\7z.exe"
$DesktopPath = "C:\Users\KP9\Desktop"
$AmJsonFileName = "AssetManifest.json"

# ----------------------------------------------------
# 脚本核心逻辑
# ----------------------------------------------------

$ApkFileName = "app-arm64-v8a-debug.apk"
$BuildDir = "build/app/outputs/flutter-apk"

# 获取 APK 文件的完整路径和当前目录
$OriginalApkRelativePath = Join-Path $BuildDir $ApkFileName
$OriginalApkFullPath = Join-Path (Get-Location) $OriginalApkRelativePath

$FinalOutputApkPath = Join-Path $DesktopPath "app-arm64-v8a-debug-injected.apk" 
$TargetDirInApk = "assets\flutter_assets\" 
$TempInjectDir = ".\temp_apk_inject"
$TargetTempDir = Join-Path $TempInjectDir $TargetDirInApk

# --- 1. 执行 Flutter DEBUG 构建 ---
Write-Host "--- 1. 正在执行 Flutter DEBUG 构建 ---"
flutter build apk --debug --target-platform android-arm64 --split-per-abi

if (-not (Test-Path $OriginalApkFullPath)) {
    Write-Error "构建失败，未找到 DEBUG APK 文件: $OriginalApkFullPath"
    exit 1
}

# --- 2. 准备临时注入目录 ---
Write-Host "--- 2. 准备临时注入目录 ---"

Remove-Item $TempInjectDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -Path $TargetTempDir -ItemType Directory -Force | Out-Null
Copy-Item $AmJsonSourcePath $TargetTempDir -Force | Out-Null

# --- 3. 注入 AssetManifest.json (最终修正) ---
Write-Host "--- 3. 正在注入 AssetManifest.json (使用 7z) ---"

if (-not (Test-Path $ZipTool)) {
    Write-Error "找不到 7z.exe 文件：$ZipTool"
    exit 1
}

# 记住当前工作目录，并切换到临时目录
$CurrentDir = Get-Location
Set-Location $TempInjectDir

# 确保 7z 只能看到 assets\flutter_assets\...，从而将其注入到 APK 内部的正确子目录。
& $ZipTool a "$OriginalApkFullPath" "$TargetDirInApk$AmJsonFileName" -tzip -r -y 

# 切换回原始目录
Set-Location $CurrentDir

# --- 4. 清理临时目录 ---
Write-Host "--- 4. 清理临时目录 ---"
Remove-Item $TempInjectDir -Recurse -Force -ErrorAction SilentlyContinue

# --- 5. 移动文件到桌面并重命名 ---
Write-Host "--- 5. 移动文件到桌面并重命名 ---"

Remove-Item $FinalOutputApkPath -ErrorAction SilentlyContinue
Move-Item $OriginalApkFullPath $FinalOutputApkPath

Write-Host "=========================================================="
Write-Host "已成功生成并注入 AssetManifest.json 到 DEBUG APK 文件："
Write-Host "$FinalOutputApkPath"
Write-Host "=========================================================="
