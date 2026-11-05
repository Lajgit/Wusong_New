$ErrorActionPreference = 'Stop'
$taskRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskKeil = 'E:/Keil5/UV4/UV4.exe'
$taskTargets = @(
    @{ Name = 'main'; Project = 'Wusong/MDK-ARM/Wusong.uvprojx'; Target = 'Wusong' },
    @{ Name = 'ball'; Project = 'Wusong_Ball/MDK-ARM/Wusong_Ball.uvprojx'; Target = 'Wusong_Ball' },
    @{ Name = 'boot'; Project = 'Bootloader_Danjie/MDK-ARM/Bootloader_F407.uvprojx'; Target = 'Bootloader_F407' }
)
$taskCatalog = @{}
$taskBuildStamp = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff')
foreach ($taskTarget in $taskTargets) {
    $taskOriginal = Join-Path $taskRoot $taskTarget.Project
    $taskTemporary = Join-Path (Split-Path $taskOriginal) (([System.IO.Path]::GetFileNameWithoutExtension($taskOriginal)) + '_verify.uvprojx')
    $taskOutput = Join-Path $PSScriptRoot ($taskTarget.Name + '-build-' + $taskBuildStamp)
    New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
    $taskProjectText = [System.IO.File]::ReadAllText($taskOriginal)
    $taskRelativeOutput = '..\..\tmp\verification\' + $taskTarget.Name + '-build-' + $taskBuildStamp + '\'
    $taskProjectText = [regex]::Replace($taskProjectText, '<OutputDirectory>[^<]*</OutputDirectory>', '<OutputDirectory>' + $taskRelativeOutput + '</OutputDirectory>')
    $taskProjectText = $taskProjectText.Replace('<BrowseInformation>1</BrowseInformation>', '<BrowseInformation>0</BrowseInformation>')
    [System.IO.File]::WriteAllText($taskTemporary, $taskProjectText, [System.Text.UTF8Encoding]::new($false))
    $taskLog = Join-Path $PSScriptRoot ('build-' + $taskTarget.Name + '-release.log')
    $taskArguments = '-r "' + $taskTemporary + '" -t "' + $taskTarget.Target + '" -o "' + $taskLog + '"'
    try {
        $taskProcess = Start-Process -FilePath $taskKeil -ArgumentList $taskArguments -WindowStyle Hidden -PassThru -Wait
        $taskLogText = [System.IO.File]::ReadAllText($taskLog)
        if ($taskLogText -notmatch '- 0 Error\(s\)') { throw ('Build failed; read ' + $taskLog) }
        $taskAxf = @(Get-Item -LiteralPath (Join-Path $taskOutput ($taskTarget.Target + '.axf')))
        $taskHex = @(Get-Item -LiteralPath (Join-Path $taskOutput ($taskTarget.Target + '.hex')))
        if ($taskAxf.Count -ne 1 -or $taskHex.Count -ne 1) { throw ('Expected one AXF and HEX in ' + $taskOutput) }
        $taskCatalog[$taskTarget.Name] = @{ axf = $taskAxf[0].FullName; hex = $taskHex[0].FullName; log = $taskLog }
        Write-Output ($taskTarget.Name + ': ' + ([regex]::Match($taskLogText, '[^\r\n]*- 0 Error\(s\)[^\r\n]*').Value))
    }
    finally {
        # 临时工程与原工程同目录，清理前校验完整路径仍位于当前工作区内。
        $taskResolvedTemporary = [System.IO.Path]::GetFullPath($taskTemporary)
        if (-not $taskResolvedTemporary.StartsWith($taskRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Temporary project escaped workspace' }
        Remove-Item -LiteralPath $taskResolvedTemporary -Force -ErrorAction SilentlyContinue
    }
}
$taskCatalog | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'release-builds.json') -Encoding utf8
