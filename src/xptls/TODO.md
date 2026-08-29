# xptls TODO

## Complete the Botan 3.13 API migration

The minimum Botan version is now 3.13.0.  The provider uses its typed IPv4 and
IPv6 subjectAltName accessors, strict-DER decoder limits, and native
CRLDistributionPoints/issuingDistributionPoint support.

Retain interoperability tests for IPv4/IPv6 SANs and CRL distribution-point
extensions.  Add a focused test proving that a complete CRL without
issuingDistributionPoint validates a certificate that has
CRLDistributionPoints.  This tracks
[Botan issue #5784](https://github.com/randombit/botan/issues/5784), which also
covered the encoder and validation issues that prompted the old compatibility
code.

The generic Raw_CSR_Extension helper remains intentional: callers may request
arbitrary X.509 extensions for which Botan has no typed class.
