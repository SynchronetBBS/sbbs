oidcfg = {};
oidcfg.issuer = 'http://'+system.host_name;
oidcfg.authorization_endpoint = oidcfg.issuer + '/oauth/auth.ssjs'
oidcfg.token_endpoint = oidcfg.issuer + '/oauth/token.ssjs'
oidcfg.jwks_uri = oidcfg.issuer + '/oauth/jwks.ssjs'
oidcfg.response_types_supported = ['code', 'id_token', 'token id_token'];
oidcfg.subject_types_supported = ['public'];
oidcfg.id_token_signing_alg_values_supported = ['RS256'];

http_reply.status='200 Hack Hack Hack';
http_reply.header['Content-Type'] = 'application/json';
write(JSON.stringify(oidcfg));
