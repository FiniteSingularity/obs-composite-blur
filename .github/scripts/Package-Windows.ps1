[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [switch] $Installer,
    [ValidateSet('All', 'Modern', 'Legacy')]
    [string] $InstallerVariant = 'All',
    [switch] $SignCode
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $StagingRoot = "${ProjectRoot}/release/${Configuration}"

    $Archives = @(
        @{
            Label = 'OBS 33+'
            StagingDir = "${StagingRoot}/${ProductName}"
            OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
        }
        @{
            Label = 'legacy'
            StagingDir = "${StagingRoot}/${ProductName}_legacy"
            OutputName = "${ProductName}-${ProductVersion}-windows-legacy-${Target}"
        }
        @{
            Label = 'portable legacy'
            StagingDir = "${StagingRoot}/${ProductName}_portable_legacy"
            OutputName = "${ProductName}-${ProductVersion}-windows-portable-legacy-${Target}"
        }
    )

    foreach ( $Archive in $Archives ) {
        if ( ! ( Test-Path -Path $Archive.StagingDir ) ) {
            throw "Staging directory '$($Archive.StagingDir)' not found. Run Build-Windows.ps1 -Configuration ${Configuration} first."
        }
    }

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.msi"
        )
    }

    Remove-Item @RemoveArgs

    if ( $SignCode ) {
        Log-Group "Signing ${ProductName} binaries..."
        $Binaries = Get-ChildItem -Path $StagingRoot -Recurse -Include '*.dll' | Select-Object -ExpandProperty FullName
        Sign-WindowsFile -FilePath $Binaries
        Log-Group
    }

    foreach ( $Archive in $Archives ) {
        Log-Group "Archiving ${ProductName} $($Archive.Label)..."
        $CompressArgs = @{
            Path = (Get-ChildItem -Path $Archive.StagingDir -Exclude "$($Archive.OutputName)*.*")
            CompressionLevel = 'Optimal'
            DestinationPath = "${ProjectRoot}/release/$($Archive.OutputName).zip"
            Verbose = ($Env:CI -ne $null)
        }
        Compress-Archive -Force @CompressArgs
        Log-Group
    }

    if ( $Installer ) {
        $InstallerArgs = @{
            Target = $Target
            Configuration = $Configuration
            Variant = $InstallerVariant
            SignCode = $SignCode
        }

        & "${ScriptHome}/Build-WindowsInstaller.ps1" @InstallerArgs

        if ( $LASTEXITCODE -ne 0 ) {
            throw "Build-WindowsInstaller.ps1 exited with code ${LASTEXITCODE}."
        }
    }
}

Package
