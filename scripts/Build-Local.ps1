# Build with an existing extracted MSVC/SDK/Qt toolchain when VS/CMake is absent.
[CmdletBinding()]
param(
    [string]$ToolsRoot = 'tmp/native-tools',
    [string]$QtRoot = 'tmp/qt-sdk',
    [string]$Output = 'build/local-resume',
    [switch]$NativeOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
# Windows PowerShell 5.1 lacks [IO.Path]::GetRelativePath; URI-based fallback with the same result.
function Get-RelativePath([string]$BaseDirectory, [string]$Path) {
    $baseUri = [Uri]((($BaseDirectory -replace '[\\/]+$', '') + [IO.Path]::DirectorySeparatorChar))
    return [Uri]::UnescapeDataString($baseUri.MakeRelativeUri([Uri]$Path).ToString()).Replace('/', [IO.Path]::DirectorySeparatorChar)
}
Push-Location $root
try {
    $tools = (Resolve-Path -LiteralPath $ToolsRoot).Path
    $qt = if ($NativeOnly) { '' } else { (Resolve-Path -LiteralPath $QtRoot).Path }
    if (!$NativeOnly -and !(Get-Command mt.exe -ErrorAction SilentlyContinue)) {
        $manifestTool = Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin" -Filter mt.exe -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq 'x64' } | Sort-Object FullName -Descending | Select-Object -First 1
        if (!$manifestTool) { throw 'Windows SDK x64 mt.exe is required for the Windows compatibility manifest.' }
        $env:PATH = $manifestTool.Directory.FullName + ';' + $env:PATH
    }
    $vc = Join-Path $tools 'msvc/VC/Tools/MSVC/14.42.34433'
    $compiler = Join-Path $vc 'bin/Hostx64/x64/cl.exe'
    if (!(Test-Path -LiteralPath $compiler)) { throw 'MSVC 14.42 compiler not found.' }
    $destination = [IO.Path]::GetFullPath((Join-Path $root $Output))
    if (!$destination.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Output must be inside the project.' }
    if (Test-Path -LiteralPath $destination) { throw 'Use a fresh, absent build output.' }
    $null = New-Item -ItemType Directory -Path $destination
    $includeVc = Get-RelativePath $root $vc
    $includeTools = Get-RelativePath $root $tools
    $includeQt = if ($NativeOnly) { '' } else { Get-RelativePath $root $qt }
    $env:INCLUDE = "$includeVc/include;$includeTools/sdk/Include/10.0.26100.0/ucrt;$includeTools/sdk/Include/10.0.26100.0/shared;$includeTools/sdk/Include/10.0.26100.0/um;$includeTools/sdk/Include/10.0.26100.0/winrt;$includeTools/sdk/Include/10.0.26100.0/cppwinrt"
    $env:LIB = "$vc/lib/x64;$tools/sdk/ucrt/x64;$tools/sdk/um/x64"
    if (!$NativeOnly) { $env:LIB += ";$qt/lib" }
    function Invoke-Checked([string]$Program, [string[]]$Arguments) {
        & $Program @Arguments
        if ($LASTEXITCODE) { throw "$Program failed: $LASTEXITCODE" }
    }
    $common = @('/nologo','/std:c++20','/Zc:__cplusplus','/EHsc','/O2','/MT','/W4','/permissive-','/utf-8','/sdl','/DNDEBUG','/DWINRT_NO_SOURCE_LOCATION','/DUNICODE','/D_UNICODE','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DWINVER=0x0A00','/D_WIN32_WINNT=0x0A00','/I','src',"/Fo:$destination/")
    $qtIncludes = @('/DQT_NO_DEBUG','/DQT_GUI_LIB','/DQT_CORE_LIB','/DQT_TESTLIB_LIB','/I',"$includeQt/include")
    foreach ($module in @('QtCore','QtGui','QtQuick','QtQml','QtQmlIntegration','QtTest')) { $qtIncludes += @('/I',"$includeQt/include/$module") }
    $link = @('/link','/NODEFAULTLIB:OLDNAMES','/DYNAMICBASE','/NXCOMPAT')
    Invoke-Checked $compiler ($common + @('/LD',"/Fe:$destination/UmbrellaTaskbarModule.dll",'src/platform/windows/explorer/TaskbarModule.cpp') + $link + @('user32.lib','comctl32.lib','advapi32.lib','shell32.lib','gdi32.lib','windowsapp.lib','d2d1.lib'))
    Invoke-Checked $compiler ($common + @('/guard:cf',"/Fe:$destination/UmbrellaTaskbarHost.exe",'src/platform/windows/explorer/TaskbarModuleHost.cpp') + $link + @('/guard:cf','user32.lib','advapi32.lib','ole32.lib','oleaut32.lib','uiautomationcore.lib','gdi32.lib','shell32.lib'))
    if ($NativeOnly) { Write-Output 'Built native taskbar host/module only; no GUI build or deployment.'; return }
    Invoke-Checked $compiler ($common + @('/guard:cf','/LD',"/Fe:$destination/UmbrellaMenuModule.dll",'src/platform/windows/explorer/TaskbarMenuModule.cpp') + $link + @('/guard:cf','user32.lib','comctl32.lib','windowsapp.lib'))
    Invoke-Checked $compiler ($common + @('/guard:cf',"/Fe:$destination/UmbrellaMenuHost.exe",'src/platform/windows/explorer/TaskbarMenuHost.cpp') + $link + @('/guard:cf','/SUBSYSTEM:WINDOWS','user32.lib','shell32.lib'))
    $version = [regex]::Match((Get-Content CMakeLists.txt -Raw), 'project\(UmbrellaShell VERSION (\d+\.\d+\.\d+)').Groups[1].Value
    "#pragma once`n#define UMBRELLA_VERSION `"$version`"" | Set-Content -LiteralPath (Join-Path $destination 'Version.h') -Encoding utf8
    $resources = '<RCC><qresource prefix="/qt/qml/Umbrella">'
    foreach ($file in @('MaterialEffect.qml','WidgetSurface.qml','WidgetMaterial.qml','MaterialText.qml','ClockWidget.qml','ClockSettings.qml','WidgetHost.qml','WidgetsPage.qml','WidgetEditor.qml','FluentCheck.qml','FluentSlider.qml','FluentButton.qml','SettingsTheme.qml','Settings.qml','TaskbarPage.qml','TaskbarConfiguration.qml','TaskbarLayout.js')) {
        $source = [Security.SecurityElement]::Escape((Join-Path $root "src/presentation/$file"))
        $resources += "<file alias=`"$file`">$source</file>"
    }
    $logo = [Security.SecurityElement]::Escape((Join-Path $root 'assets/umbrella.png'))
    $resources += "</qresource><qresource prefix=`"/`"><file alias=`"assets/umbrella.png`">$logo</file></qresource></RCC>"
    $shaderBaker = if (Test-Path "$qt/bin/qsb.exe") { "$qt/bin/qsb.exe" } else { "tmp/pyside683/PySide6/qsb.exe" }
    Invoke-Checked $shaderBaker @('--qt6','-o',"$destination/glyph.frag.qsb",'src/presentation/shaders/glyph.frag')
    $shader = [Security.SecurityElement]::Escape((Join-Path $destination 'glyph.frag.qsb'))
    $resources = $resources.Replace('</RCC>', "<qresource prefix=`"/materials`"><file alias=`"glyph.frag.qsb`">$shader</file></qresource></RCC>")
    $qrc = Join-Path $destination 'settings.qrc'
    $resources | Set-Content -LiteralPath $qrc -Encoding utf8
    Invoke-Checked "$qt/bin/moc.exe" @('src/app/TaskbarSettings.h','-I','src','-I',"$qt/include",'-I',"$qt/include/QtCore",'-o',"$destination/moc_TaskbarSettings.cpp")
    Invoke-Checked "$qt/bin/rcc.exe" @($qrc,'-o',"$destination/qrc_settings.cpp")
    Invoke-Checked "$qt/bin/moc.exe" @('src/platform/windows/SystemTheme.h','-I','src','-I',"$qt/include",'-I',"$qt/include/QtCore",'-o',"$destination/moc_SystemTheme.cpp")
    $platform = @('src/platform/windows/DesktopLayer.cpp','src/platform/windows/SystemTheme.cpp',"$destination/moc_SystemTheme.cpp", 'src/platform/windows/WindowBackdrop.cpp','src/platform/windows/SettingsWindowControl.cpp','src/platform/windows/TaskbarRuntime.cpp','src/platform/windows/TaskbarState.cpp','src/platform/windows/TaskbarAppearance.cpp','src/platform/windows/TaskbarModuleSession.cpp')
    Invoke-Checked "$qt/bin/moc.exe" @('src/app/WidgetsModel.h','-I','src','-I',"$qt/include",'-I',"$qt/include/QtCore",'-o',"$destination/moc_WidgetsModel.cpp")
    Invoke-Checked "$qt/bin/moc.exe" @('src/app/ClockModel.h','-I','src','-I',"$qt/include",'-I',"$qt/include/QtCore",'-o',"$destination/moc_ClockModel.cpp")
    $model = @('src/app/ClockModel.cpp',"$destination/moc_ClockModel.cpp",'src/app/WidgetsModel.cpp',"$destination/moc_WidgetsModel.cpp",'src/app/TaskbarSettings.cpp',"$destination/moc_TaskbarSettings.cpp")
    $libraries = @('Qt6Core.lib','Qt6Gui.lib','Qt6Quick.lib','Qt6Qml.lib','dwmapi.lib','shell32.lib','user32.lib','gdi32.lib','advapi32.lib','ole32.lib','oleaut32.lib','oleacc.lib','propsys.lib','windowsapp.lib')
    Invoke-Checked $compiler ($common + $qtIncludes + @('/guard:cf','/I',$destination,"/Fe:$destination/UmbrellaShell.exe",'src/app/main.cpp') + $model + $platform + @("$destination/qrc_settings.cpp") + $link + $libraries + @('/guard:cf','/SUBSYSTEM:WINDOWS','/ENTRY:mainCRTStartup') + @('/MANIFEST:EMBED','/MANIFESTINPUT:src/platform/windows/WindowsCompatibility.manifest'))
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_policy_tests.exe",'tests/update_policy_tests.cpp') + $link)
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_placement_tests.exe",'tests/taskbar_placement_tests.cpp') + $link)
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_snapshot_protocol_tests.exe",'tests/taskbar_snapshot_protocol_tests.cpp') + $link)
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_location_override_tests.exe",'tests/taskbar_location_override_tests.cpp') + $link)
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_reorder_tests.exe",'tests/taskbar_reorder_tests.cpp') + $link)
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_xaml_layout_tests.exe",'tests/taskbar_xaml_layout_tests.cpp') + $link + @('/MANIFEST:NO','windowsapp.lib','user32.lib','shell32.lib','d2d1.lib','gdi32.lib'))
    Copy-Item -LiteralPath tests/taskbar_xaml_layout_tests.manifest -Destination "$destination/umbrella_xaml_layout_tests.exe.manifest"
    Invoke-Checked $compiler ($common + $qtIncludes + @("/Fe:$destination/umbrella_session_tests.exe",'tests/taskbar_session_tests.cpp','src/platform/windows/TaskbarModuleSession.cpp') + $link + $libraries)
    Invoke-Checked $compiler ($common + @("/Fe:$destination/umbrella_menu_patch_tests.exe",'tests/taskbar_menu_patch_tests.cpp') + $link)
    Invoke-Checked $compiler ($common + $qtIncludes + @("/Fe:$destination/umbrella_settings_lifetime_tests.exe",'tests/settings_window_lifetime_tests.cpp') + $link + @('Qt6Network.lib','Qt6Core.lib','Qt6Gui.lib','user32.lib'))
    if (Test-Path tests/resume_desktop_tests.cpp) {
        Invoke-Checked $compiler ($common + $qtIncludes + @("/Fe:$destination/umbrella_resume_desktop_tests.exe",'tests/resume_desktop_tests.cpp') + $model + $platform + $link + $libraries)
    }
    Invoke-Checked $compiler ($common + $qtIncludes + @("/Fe:$destination/umbrella_recovery_guard_tests.exe",'tests/taskbar_recovery_guard_tests.cpp') + $model + $platform + $link + $libraries)
    Invoke-Checked $compiler ($common + $qtIncludes + @("/Fe:$destination/umbrella_configuration_desktop_test.exe",'tests/taskbar_configuration_desktop_test.cpp') + $model + $platform + $link + $libraries + @('Qt6Test.lib','ole32.lib','oleaut32.lib','uiautomationcore.lib'))
    Write-Output "Built $destination using MSVC and Qt $(& "$qt/bin/qmake.exe" -query QT_VERSION). Deployment is a separate step."
} finally { Pop-Location }
