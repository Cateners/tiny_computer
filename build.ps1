param(
    [Parameter(Mandatory=$true, ValueFromRemainingArguments=$true)]
    [ValidateSet("xfce", "lxqt", "gxde")]
    [string[]]$DesktopEnvs,
    
    [string]$NameSuffix
)

# 设置路径
$SourceDir = "C:\Users\29513\Downloads"
$AssetsDir = "assets"

# 分割文件函数 - 使用xaa, xab, xac...命名
function Split-File {
    param(
        [string]$Path,
        [long]$PartSizeBytes,
        [string]$DestinationPath
    )
    
    $stream = [System.IO.File]::OpenRead($Path)
    $buffer = New-Object byte[] $PartSizeBytes
    $partNumber = 0
    
    try {
        while (($bytesRead = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            # 生成类似xaa, xab, xac...的文件名
            $partName = Get-SplitFileName $partNumber
            $partPath = Join-Path $DestinationPath $partName
            
            $partStream = [System.IO.File]::OpenWrite($partPath)
            try {
                $partStream.Write($buffer, 0, $bytesRead)
            } finally {
                $partStream.Close()
            }
            
            Write-Host "创建分片: $partName"
            $partNumber++
        }
    } finally {
        $stream.Close()
    }
    
    return $partNumber
}

# 生成split风格的文件名 (xaa, xab, xac, ..., xaz)
function Get-SplitFileName {
    param([int]$index)
    
    # 计算字母偏移量（0-25对应a-z）
    $charOffset = $index % 26
    # 将数字转换为对应的字母字符
    $char = [char]([byte][char]'a' + $charOffset)
    
    return "xa$char"
}

# 处理每个桌面环境
foreach ($DesktopEnv in $DesktopEnvs) {
    Write-Host "`n开始处理 $DesktopEnv 桌面环境..." -ForegroundColor Green
    
    # 设置文件路径
    $TarFile = "debian-$DesktopEnv.tar.xz"
    $SourcePath = Join-Path $SourceDir $TarFile
    
    # 检查源文件是否存在
    if (-not (Test-Path $SourcePath)) {
        Write-Error "错误：找不到文件 $SourcePath"
        continue
    }
    
    # 删除assets文件夹中已有的xa*文件
    if (Test-Path $AssetsDir) {
        Write-Host "正在清理assets文件夹中的旧文件..."
        Get-ChildItem -Path $AssetsDir -Filter "xa*" | Remove-Item -Force
    } else {
        Write-Host "创建assets文件夹..."
        New-Item -ItemType Directory -Path $AssetsDir | Out-Null
    }
    
    # 分割文件 (98MB = 98 * 1024 * 1024 = 102760448 bytes)
    Write-Host "正在分割 $TarFile 文件..."
    $partCount = Split-File -Path $SourcePath -PartSizeBytes 102760448 -DestinationPath $AssetsDir
    
    Write-Host "文件分割完成，共创建 $partCount 个分片文件"
    
    # 运行Flutter构建
    Write-Host "正在运行Flutter构建..."
    flutter build apk --target-platform android-arm64 --split-per-abi --obfuscate --split-debug-info=tiny_computer/sdi
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "错误：Flutter构建失败"
        continue
    }
    
    # 构建APK文件名
    $ApkBaseName = "tiny-computer-$DesktopEnv"
    if (-not [string]::IsNullOrEmpty($NameSuffix)) {
        $ApkBaseName += "-$NameSuffix"
    }
    
    # 重命名APK和SHA1文件
    $ApkSource = "build\app\outputs\flutter-apk\app-arm64-v8a-release.apk"
    $Sha1Source = "build\app\outputs\flutter-apk\app-arm64-v8a-release.apk.sha1"
    
    if (Test-Path $ApkSource) {
        Rename-Item -Path $ApkSource -NewName "$ApkBaseName.apk"
        Write-Host "已重命名APK文件: $ApkBaseName.apk"
    } else {
        Write-Error "错误：找不到APK文件 $ApkSource"
        continue
    }
    
    if (Test-Path $Sha1Source) {
        Rename-Item -Path $Sha1Source -NewName "$ApkBaseName.apk.sha1"
        Write-Host "已重命名SHA1文件: $ApkBaseName.apk.sha1"
    } else {
        Write-Warning "警告：找不到SHA1文件 $Sha1Source"
    }
    
    Write-Host "$DesktopEnv 处理完成！" -ForegroundColor Green
}

Write-Host "`n所有桌面环境处理完成！" -ForegroundColor Cyan

# 既然是开源，我认为应该把prompt开源出来才算，毕竟这个脚本更像编译后的产物，而不是源代码本身。

# 帮我写一个自动化脚本，做以下几件事：
# 1. 脚本所在目录是项目的根目录，脚本应该运行在windows电脑上，接收一个参数，这个参数的值会是xfce, lxqt或gxde。
# 2. 在C:\Users\29513\Downloads文件夹有debian-xfce.tar.xz，debian-lxqt.tar.xz和debian-gxde.tar.xz，需要根据之前的参数对应选择，然后分成98MB的小份，命名为xa*（就像linux上的split -b 98M debian.tar.xz），放到项目的assets文件夹。注意这个文件夹可能有之前残留的xa*文件，需要先彻底删除这些xa*文件。
# 3. 然后在当前目录运行flutter build apk --target-platform android-arm64 --split-per-abi --obfuscate --split-debug-info=tiny_computer/sdi编译。
# 4. 在build\app\outputs\flutter-apk文件夹会有app-arm64-v8a-release.apk和app-arm64-v8a-release.apk.sha1两个文件，需要重命名为tiny-computer-xfce.apk和tiny-computer-xfce.apk.sha1（以xfce为例，具体名称根据参数来定）

# 直接写成一个ps1脚本行吗

# 请再添加一些功能：首先可以传入多个选项，比如传入xfce lxqt就可以自动进行这两个构建；其实需要一个新参数允许在生成的apk名字加入后缀，比如添加targetSdk35后缀，就会生成tiny-computer-xfce-targetSdk35.apk和tiny-computer-xfce-targetSdk35.apk.sha1

# xa*文件的命名不对。要按照split命令默认的那样，命名为xaa，xab，xac... 另外我确定分割后的文件数量不多，不会超过xaz。

# Cannot convert value "97" to type "System.Char". Error: "Invalid cast from 'Decimal' to 'Char'."
# At C:\Users\29513\FlutterProjects\tiny_computer\build.ps1:52 char:5
# +     $firstChar = [char](97 + [math]::Floor($index / 26))  # a-z
# +     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#     + CategoryInfo          : InvalidArgument: (:) [], RuntimeException
#     + FullyQualifiedErrorId : InvalidCastIConvertible
