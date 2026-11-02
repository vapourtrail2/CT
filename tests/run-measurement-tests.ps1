param(
    [string]$SdkRoot,
    [string]$QtInstall = 'C:\Qt\Qt5.14.2\5.14.2\msvc2017_64',
    [string]$MSBuild = 'C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe',
    [switch]$WithSdk
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $projectRoot 'MVVCVTK'
if (!$SdkRoot) {
    $SdkRoot = Join-Path $projectRoot '2026.10.23-git.9a5b53c08ddd-win-x64'
}
$testRoot = Join-Path $projectRoot 'out\measurement-tests'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'MVVCVTK.vcxproj') -Raw
$project = [regex]::Replace($project, '    <ClCompile Include="[^"]+"\s*(?:/>|>.*?</ClCompile>)', '', 'Singleline')
$project = [regex]::Replace($project, '    <QtMoc Include="[^"]+"\s*(?:/>|>.*?</QtMoc>)', '', 'Singleline')
$project = $project.Replace('<TargetName>GviewCT</TargetName>', '<TargetName>MeasurementIntegrationTests</TargetName>')
$project = $project.Replace('Project="MVVCVTK.Sdk.props"', ('Project="' + $sourceRoot + '\MVVCVTK.Sdk.props"'))
$project = $project.Replace("Exists('MVVCVTK.Sdk.props')", ("Exists('" + $sourceRoot + "\MVVCVTK.Sdk.props')"))
$project = $project.Replace('$(ProjectDir)c_ui\QtStlCompat.h', ($sourceRoot + '\c_ui\QtStlCompat.h'))
$project = $project.Replace('<AdditionalIncludeDirectories>', ('<AdditionalIncludeDirectories>' + $sourceRoot + ';'))
$sources = @('c_ui\context\VolumeManager.cpp', 'c_ui\panels\SceneTreeModel.cpp', 'measure\MeasurementSession.cpp', 'measure\MeasurementGeometry.cpp', 'measure\MeasurementToolDefinition.cpp', 'measure\MeasurementView.cpp', 'measure\MeasureViewAdapter.cpp', 'measure\MeasurementInteractionHandler.cpp', 'measure\MeasurementOverlayStrategy.cpp', 'measure\EdgeCaptureController.cpp', 'measure\ZcEdgeAlgorithm.cpp', 'measure\MainMeasurementFeature.cpp')
$entries = foreach ($file in $sources) { '    <ClCompile Include="' + (Join-Path $sourceRoot $file) + '" />' }
$entries += '    <ClCompile Include="' + (Join-Path $PSScriptRoot 'measurement-integration-tests.cpp') + '" />'
$project = $project.Replace('</Project>', ("  <ItemGroup>`n" + ($entries -join "`n") + "`n  </ItemGroup>`n</Project>"))
$testProject = Join-Path $testRoot 'MeasurementIntegrationTests.vcxproj'
Set-Content -LiteralPath $testProject -Value $project -Encoding utf8
$bin = Join-Path $testRoot 'bin\'
$obj = Join-Path $testRoot 'obj\'
& $MSBuild $testProject /t:Build /p:Configuration=Release /p:Platform=x64 "/p:QtInstall=$QtInstall" "/p:MVVCVTKSdkRoot=$SdkRoot" "/p:CTRoot=$projectRoot\" "/p:OutDir=$bin" "/p:IntDir=$obj" /p:QtDeploy=false /m:2 /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$env:PATH = (Join-Path $QtInstall 'bin') + ';' + $env:PATH
$executable = Join-Path $bin 'MeasurementIntegrationTests.exe'
if ($WithSdk) { & $executable --sdk } else { & $executable }
exit $LASTEXITCODE
