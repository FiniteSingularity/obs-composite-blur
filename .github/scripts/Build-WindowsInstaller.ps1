[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [ValidateSet('All', 'Modern', 'Legacy')]
    [string] $Variant = 'All',
    [switch] $SignCode
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Installer build requires a 64-bit system."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The installer build script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function ConvertTo-Rtf {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Destination
    )

    $Text = Get-Content -Path $Path -Raw
    $Text = $Text -replace '\\', '\\' -replace '\{', '\{' -replace '\}', '\}'
    $Lines = ($Text -split "`r?`n") | ForEach-Object { "${_}\par" }

    $Rtf = @(
        '{\rtf1\ansi\ansicpg1252\deff0{\fonttbl{\f0\fmodern\fcharset0 Courier New;}}'
        '\f0\fs16'
        $Lines
        '}'
    ) -join "`r`n"

    Set-Content -Path $Destination -Value $Rtf -Encoding ascii -NoNewline
}

function Build-Installer {
    trap {
        Pop-Location -Stack InstallerTemp -ErrorAction 'SilentlyContinue'
        Write-Error $_
        Log-Group
        exit 2
    }

    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductDisplayName = if ( $BuildSpec.displayName ) { $BuildSpec.displayName } else { $ProductName }
    $ProductVersion = $BuildSpec.version
    $ProductAuthor = $BuildSpec.author
    $ProductWebsite = $BuildSpec.website

    if ( $ProductVersion -notmatch '^(?<msi>\d+\.\d+\.\d+)' ) {
        throw "Version '${ProductVersion}' in buildspec.json is not of the form major.minor.patch[-suffix]."
    }
    $MsiVersion = $Matches.msi

    $BuildDir = [System.IO.Path]::GetFullPath("${ProjectRoot}/build_${Target}")
    $InstallerDir = "${BuildDir}\installer"
    $ReleaseDir = [System.IO.Path]::GetFullPath("${ProjectRoot}/release")
    $StagingRoot = "${ReleaseDir}\${Configuration}"
    $SourceDir = [System.IO.Path]::GetFullPath("${ProjectRoot}/cmake/windows/resources/installer")

    #   banner.bmp  493 x 58 px   24-bit BMP
    #   dialog.bmp  493 x 312 px  24-bit BMP
    #   icon.ico    multi-size ICO
    $BrandingDefines = @()
    $BrandingFiles = @{
        BannerImage = "${SourceDir}\images\banner.bmp"
        DialogImage = "${SourceDir}\images\dialog.bmp"
        ProductIcon = "${SourceDir}\images\icon.ico"
    }

    foreach ( $Branding in $BrandingFiles.GetEnumerator() ) {
        if ( Test-Path -Path $Branding.Value ) {
            $BrandingDefines += @('-d', "$($Branding.Key)=$($Branding.Value)")
        } else {
            Log-Warning "Branding file '$($Branding.Value)' not found, using the WiX default."
        }
    }

    $Installers = @()

    if ( $Variant -in 'All', 'Modern' ) {
        $Installers += @{
            Key = 'modern'
            Label = 'OBS 33+'
            StagingDir = "${StagingRoot}\${ProductName}"
            OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
            Sources = @(
                "${SourceDir}\Package-Modern.wxs"
                "${SourceDir}\WixUI_ObsScope.wxs"
                "${SourceDir}\ObsScopeDlg.wxs"
            )
            Localization = @()
        }
    }

    if ( $Variant -in 'All', 'Legacy' ) {
        $Installers += @{
            Key = 'legacy'
            Label = 'legacy (pre-OBS 33)'
            StagingDir = "${StagingRoot}\${ProductName}_portable_legacy"
            OutputName = "${ProductName}-${ProductVersion}-windows-legacy-${Target}"
            Sources = @(
                "${SourceDir}\Package-Legacy.wxs"
            )
            Localization = @(
                "${SourceDir}\Package-Legacy.en-us.wxl"
            )
        }
    }

    foreach ( $Installer in $Installers ) {
        if ( ! ( Test-Path -Path $Installer.StagingDir ) ) {
            throw "Staging directory '$($Installer.StagingDir)' not found. Run Build-Windows.ps1 -Configuration ${Configuration} first."
        }
    }

    New-Item -ItemType Directory -Path $InstallerDir -Force | Out-Null

    Push-Location -Stack InstallerTemp
    Set-Location -Path $ProjectRoot

    Log-Group "Restoring WiX toolset..."
    Invoke-External dotnet tool restore
    $WixVersion = ((& dotnet wix --version) | Select-Object -First 1).Trim() -replace '\+.*$', ''
    Invoke-External dotnet wix extension add "WixToolset.UI.wixext/${WixVersion}"
    Log-Group

    Log-Group "Generating license text..."
    $LicenseRtf = "${InstallerDir}\License.rtf"
    ConvertTo-Rtf -Path "${ProjectRoot}/LICENSE" -Destination $LicenseRtf
    Log-Group

    foreach ( $Installer in $Installers ) {
        $OutputFile = "${ReleaseDir}\$($Installer.OutputName).msi"

        Log-Group "Building $($Installer.Label) installer $($Installer.OutputName).msi..."
        Remove-Item -Path $OutputFile -ErrorAction SilentlyContinue

        $WixArgs = @(
            'wix', 'build'
            '-arch', 'x64'
            '-ext', 'WixToolset.UI.wixext'
            '-intermediateFolder', "${InstallerDir}\obj\$($Installer.Key)"
            '-pdbtype', 'none'
            '-d', "ProductName=${ProductName}"
            '-d', "ProductDisplayName=${ProductDisplayName}"
            '-d', "ProductVersion=${ProductVersion}"
            '-d', "MsiVersion=${MsiVersion}"
            '-d', "ProductAuthor=${ProductAuthor}"
            '-d', "ProductWebsite=${ProductWebsite}"
            '-d', "StagingDir=$($Installer.StagingDir)"
            '-d', "LicenseRtf=${LicenseRtf}"
            '-o', $OutputFile
        )

        $WixArgs += $BrandingDefines

        foreach ( $Localization in $Installer.Localization ) {
            $WixArgs += @('-loc', $Localization)
        }

        if ( $DebugPreference -eq 'Continue' ) {
            $WixArgs += '-v'
        }

        $WixArgs += $Installer.Sources

        Invoke-External dotnet @WixArgs

        if ( $SignCode ) {
            Sign-WindowsFile -FilePath $OutputFile
        }

        Log-Group
    }

    Pop-Location -Stack InstallerTemp
}

Build-Installer
