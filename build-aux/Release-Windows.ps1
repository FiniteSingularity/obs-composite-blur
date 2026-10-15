[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [ValidateSet('All', 'Modern', 'Legacy')]
    [string] $Variant = 'All',
    [switch] $SkipBuild,
    [switch] $SignCode
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path -Path "$PSScriptRoot/.."
$Scripts = "${ProjectRoot}/.github/scripts"

if ( ! $SkipBuild ) {
    & "${Scripts}/Build-Windows.ps1" -Configuration $Configuration
    if ( $LASTEXITCODE -ne 0 ) { exit $LASTEXITCODE }
}

$PackageArgs = @{
    Configuration = $Configuration
    Installer = $true
    InstallerVariant = $Variant
    SignCode = $SignCode
}

& "${Scripts}/Package-Windows.ps1" @PackageArgs
exit $LASTEXITCODE
