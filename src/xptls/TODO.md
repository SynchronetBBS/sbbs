# xptls TODO

## Require Botan 3.13.0

Once Botan 3.13.0 is released and available on the supported platforms,
consider raising xptls's minimum Botan version from 3.6 to 3.13.0.  Before
doing so, confirm that the release contains the expected public APIs and that
the supported-platform packages have caught up.

That version should let the Botan provider replace compatibility code for:

- IP subjectAltName access.  Botan 3.11 lacks the IPv4Address and IPv6Address
  interfaces, so xptls currently decodes the subjectAltName extension to
  return IPv4 and IPv6 addresses.
- Strict DER decoding.  Botan 3.11 lacks BER_Decoder::Limits::DER(), so code
  that accepts externally supplied DER cannot request Botan's strict DER
  limits through the public decoder API.
- CRLDistributionPoints and issuingDistributionPoint.  The current Botan
  interfaces expose CRL_Distribution_Points and its Distribution_Point for
  some of this work, but issuingDistributionPoint encoding still requires
  the local xp_ca_issuing_distribution_point extension.  Replace that with
  Botan's CRL_Issuing_Distribution_Point support when it is available.  Track
  [Botan issue #5784](https://github.com/randombit/botan/issues/5784), which
  covers both the incorrect requirement for issuingDistributionPoint during
  CRL validation and the missing public encoder.

After increasing the minimum, remove the associated raw extension encoders
and decoders from xp_ca_botan3.cpp and retain interoperability tests for both
IPv4/IPv6 SANs and CRL distribution-point extensions.

Soon, but separately from the current CI compatibility fix, add a focused
interoperability test proving that a complete CRL without
issuingDistributionPoint validates a certificate that has
CRLDistributionPoints.  Keep that test when moving to Botan's native
CRL_Issuing_Distribution_Point implementation.
