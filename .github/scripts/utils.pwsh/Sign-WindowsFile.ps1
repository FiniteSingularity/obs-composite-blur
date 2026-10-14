function Get-PeArchitecture {

    param(
        [Parameter(Mandatory)]
        [string] $Path
    )

    $Stream = [System.IO.File]::OpenRead($Path)
    try {
        $Reader = New-Object System.IO.BinaryReader($Stream)
        $Stream.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
        $PeOffset = $Reader.ReadInt32()
        $Stream.Seek($PeOffset + 4, [System.IO.SeekOrigin]::Begin) | Out-Null
        $Machine = $Reader.ReadUInt16()
    } finally {
        $Stream.Dispose()
    }

    switch ( $Machine ) {
        0x014c { return 'x86' }
        0x8664 { return 'x64' }
        0xAA64 { return 'arm64' }
        default { throw "Unknown PE machine type 0x$($Machine.ToString('X4')) in '${Path}'." }
    }
}

function Get-ScSignTool {
    param(
        [Parameter(Mandatory)]
        [string] $CacheDir,
        [ValidateSet('x86', 'x64')]
        [string] $Architecture = 'x86'
    )

    if ( $env:SCSIGNTOOL ) {
        if ( ! ( Test-Path -Path $env:SCSIGNTOOL ) ) {
            throw "SCSIGNTOOL is set to '${env:SCSIGNTOOL}' but that file does not exist."
        }
        return (Resolve-Path -Path $env:SCSIGNTOOL).Path
    }

    $Tool = Join-Path -Path $CacheDir -ChildPath "${Architecture}/ScSignTool.exe"

    if ( ! ( Test-Path -Path $Tool ) ) {
        $Url = 'https://www.mgtek.com/files/smartcardtools.zip'
        $Zip = Join-Path -Path $CacheDir -ChildPath 'smartcardtools.zip'

        Log-Information "Downloading ScSignTool from ${Url}..."
        New-Item -ItemType Directory -Path $CacheDir -Force | Out-Null
        Invoke-WebRequest -Uri $Url -OutFile $Zip -MaximumRetryCount 5 -RetryIntervalSec 5
        Expand-Archive -Path $Zip -DestinationPath $CacheDir -Force

        if ( ! ( Test-Path -Path $Tool ) ) {
            throw "smartcardtools.zip did not contain ${Architecture}/ScSignTool.exe."
        }
    }

    return (Resolve-Path -Path $Tool).Path
}

function Resolve-SignTool {
    $SignTool = Get-Command -Name signtool.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source

    if ( ! $SignTool ) {
        $Candidates = @(
            "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe"
            "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x86/signtool.exe"
        )

        $SignTool = Get-ChildItem -Path $Candidates -ErrorAction SilentlyContinue |
            Sort-Object -Property FullName -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if ( ! $SignTool ) {
        throw 'signtool.exe not found on PATH or in the Windows 10/11 SDK. Install the Windows SDK "Windows SDK Signing Tools for Desktop Apps" component.'
    }

    $env:PATH = "$(Split-Path -Path $SignTool -Parent);${env:PATH}"

    return $SignTool
}

function Sign-WindowsFile {
    param(
        [Parameter(Mandatory)]
        [string[]] $FilePath,
        [string] $ToolCacheDir = (Join-Path -Path (Resolve-Path -Path "$PSScriptRoot/../../..") -ChildPath 'build_x64/smartcardtools')
    )

    if ( ! ( Test-Path function:Log-Information ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    $SigningPin = $env:SIGNING_KEY_PIN
    $SigningOrg = $env:SIGNING_ORG
    $TimestampUrl = if ( $env:SIGNING_TIMESTAMP_URL ) { $env:SIGNING_TIMESTAMP_URL } else { 'http://ts.ssl.com' }

    if ( ! $SigningPin -or ! $SigningOrg ) {
        throw 'Sign-WindowsFile: SIGNING_KEY_PIN and SIGNING_ORG must be set in the environment.'
    }

    $SignTool = Resolve-SignTool
    $Architecture = Get-PeArchitecture -Path $SignTool

    if ( $Architecture -notin 'x86', 'x64' ) {
        throw "signtool.exe at '${SignTool}' is ${Architecture}; ScSignTool is only available for x86 and x64."
    }

    $Tool = Get-ScSignTool -CacheDir $ToolCacheDir -Architecture $Architecture

    $Files = @($FilePath | ForEach-Object { (Resolve-Path -Path $_).Path })

    Log-Information "Signing $($Files.Count) file(s) as '${SigningOrg}' with $(Split-Path -Path $Tool -Leaf) (${Architecture}, signtool: ${SignTool})..."
    foreach ( $File in $Files ) {
        Log-Information "  $(Split-Path -Path $File -Leaf)"
    }

    # Deliberately not Invoke-External: it echoes its full argument list in debug mode, which
    # would put the PIN in the build log.
    $_EAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'

    & $Tool -pin $SigningPin sign /fd sha256 /tr $TimestampUrl /td sha256 /n $SigningOrg @Files
    $Result = $LASTEXITCODE

    $ErrorActionPreference = $_EAP

    if ( $Result -ne 0 ) {
        throw "ScSignTool exited with non-zero code ${Result}."
    }

    foreach ( $File in $Files ) {
        $Signature = Get-AuthenticodeSignature -FilePath $File

        if ( $Signature.Status -ne 'Valid' ) {
            throw "Signature on '${File}' is $($Signature.Status): $($Signature.StatusMessage)"
        }

        Log-Information "  OK  $(Split-Path -Path $File -Leaf)  ($($Signature.SignerCertificate.Subject))"
    }
}
