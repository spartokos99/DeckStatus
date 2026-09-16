param([Parameter(Mandatory = $true)][string]$OutputFile)
$ErrorActionPreference = 'Stop'
# Ephemeral test certificate: never added to a Windows certificate/trust store.
$rsa = [System.Security.Cryptography.RSACng]::new(2048)
$certificate = $null
try {
    $request = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
        'CN=deckstatus.example', $rsa,
        [System.Security.Cryptography.HashAlgorithmName]::SHA256,
        [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $names = [System.Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
    $names.AddDnsName('deckstatus.example')
    $request.CertificateExtensions.Add($names.Build())
    $certificate = $request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-1), [DateTimeOffset]::UtcNow.AddDays(1))
    [System.IO.File]::WriteAllBytes($OutputFile, $certificate.Export(
        [System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx, 'proxy-fixture'))
} finally {
    if ($certificate) { $certificate.Dispose() }
    $rsa.Dispose()
}
