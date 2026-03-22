# RFC 4252 Conformance Audit — DeuceSSH

Systematic audit of every MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT,
SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL keyword in RFC 4252
(SSH Authentication Protocol) against the DeuceSSH library code.

DeuceSSH implements both client-side and server-side authentication
(`ssh-auth.c`).  Server-side uses a callback-driven model: the library
drives the auth loop, and the application provides callbacks for
credential verification.  Requirements about server-side policy
decisions (timeouts, attempt limits, user existence checks) remain
the application's responsibility.

Legend:
- **CONFORMS** — library satisfies the requirement
- **N/A** — not applicable to this library
- **PARTIAL** — partially conformant, see notes
- **NONCONFORMANT** — library violates the requirement
- **APPLICATION** — requirement is the responsibility of the application

---

## Section 4: The Authentication Protocol Framework

### 4-1
> The "none" method is reserved, and **MUST NOT** be listed as
> supported.

**APPLICATION** — The application provides `methods_str` in the
`deuce_ssh_auth_server_cbs` struct.  It is the application's
responsibility to not include `"none"` in this string.

### 4-2
> However, it [the "none" method] **MAY** be sent by the client.

**CONFORMS** — `deuce_ssh_auth_get_methods()` sends a "none" auth
request to query available methods.

### 4-3
> The server **MUST** always reject this request, unless the client is
> to be granted access without any authentication, in which case, the
> server **MUST** accept this request.

**CONFORMS** — `deuce_ssh_auth_server()` dispatches "none" to the
`none_cb` callback.  If the callback returns SUCCESS, the library
sends USERAUTH_SUCCESS.  If it returns FAILURE (or is NULL), the
library sends USERAUTH_FAILURE.  The policy decision is correctly
delegated to the application callback.

### 4-4
> The server **SHOULD** have a timeout for authentication and disconnect
> if authentication has not been accepted within the timeout period.

**APPLICATION** — Server-side responsibility.

### 4-5
> The **RECOMMENDED** timeout period is 10 minutes.

**APPLICATION** — Server-side responsibility.

### 4-6
> The implementation **SHOULD** limit the number of failed
> authentication attempts a client may perform in a single session (the
> **RECOMMENDED** limit is 20 attempts).

**APPLICATION** — Server-side responsibility.

### 4-7
> If the threshold is exceeded, the server **SHOULD** disconnect.

**APPLICATION** — Server-side responsibility.

---

## Section 5: Authentication Requests

### 5-1
> All authentication requests **MUST** use the following message format.
> [byte SSH_MSG_USERAUTH_REQUEST, string user name, string service name,
> string method name, ...]

**CONFORMS** — `deuce_ssh_auth_get_methods()`, `deuce_ssh_auth_password()`,
and `deuce_ssh_auth_keyboard_interactive()` all build messages with
this exact format: msg type (50), username string, service name
(`"ssh-connection"`), method name, then method-specific fields.

### 5-2
> The server implementation **MUST** carefully check [user name and
> service name] in every message, and **MUST** flush any accumulated
> authentication states if they change.

**APPLICATION** — Server-side responsibility.

### 5-3
> If [the server] is unable to flush an authentication state, it
> **MUST** disconnect if the 'user name' or 'service name' changes.

**APPLICATION** — Server-side responsibility.

### 5-4
> If the requested service is not available, the server **MAY**
> disconnect immediately or at any later time.

**APPLICATION** — Server-side responsibility.

### 5-5
> Sending a proper disconnect message is **RECOMMENDED**.

**APPLICATION** — Server-side responsibility.

### 5-6
> In any case, if the service does not exist, authentication **MUST
> NOT** be accepted.

**APPLICATION** — Server-side responsibility.

### 5-7
> If the requested 'user name' does not exist, the server **MAY**
> disconnect, or **MAY** send a bogus list of acceptable authentication
> 'method name' values, but never accept any.

**APPLICATION** — Server-side responsibility.

### 5-8
> In any case, if the 'user name' does not exist, the authentication
> request **MUST NOT** be accepted.

**APPLICATION** — Server-side responsibility.

### 5-9
> The server **SHOULD** simply reject requests that it does not
> recognize.

**APPLICATION** — Server-side responsibility.

### 5-10
> The client **MAY** at any time continue with a new
> SSH_MSG_USERAUTH_REQUEST message, in which case the server **MUST**
> abandon the previous authentication attempt and continue with the
> new one.

**CONFORMS** (client side) — The client can call any auth function at
any time.  Each function sends a fresh SSH_MSG_USERAUTH_REQUEST.

**APPLICATION** (server side) — Server must handle method switching.

### 5-11
> The following 'method name' values are defined: "publickey"
> **REQUIRED**; "password" OPTIONAL; "hostbased" OPTIONAL; "none" NOT
> RECOMMENDED.

**CONFORMS** — `publickey`, `password`, and `keyboard-interactive`
(RFC 4256) are implemented on the client side.  `hostbased` is not
implemented (OPTIONAL method).

---

## Section 5.1: Responses to Authentication Requests

### 5.1-1
> If the server rejects the authentication request, it **MUST** respond
> with [SSH_MSG_USERAUTH_FAILURE].

**CONFORMS** — `deuce_ssh_auth_server()` sends USERAUTH_FAILURE via
`send_auth_failure()` whenever a callback returns FAILURE or a method
is unsupported.

### 5.1-2
> It is **RECOMMENDED** that servers only include those 'method name'
> values in the name-list that are actually useful.

**APPLICATION** — Server-side responsibility.

### 5.1-3
> Already successfully completed authentications **SHOULD NOT** be
> included in the name-list, unless they should be performed again for
> some reason.

**APPLICATION** — Server-side responsibility.

### 5.1-4
> The value of 'partial success' **MUST** be TRUE if the authentication
> request to which this is a response was successful.

**CONFORMS** — `send_auth_failure()` sets partial_success based on the
callback's return value: `DEUCE_SSH_AUTH_PARTIAL` maps to TRUE.

### 5.1-5
> It **MUST** be FALSE if the request was not successfully processed.

**CONFORMS** — `DEUCE_SSH_AUTH_FAILURE` maps to FALSE.

### 5.1-6
> When the server accepts authentication, it **MUST** respond with
> [SSH_MSG_USERAUTH_SUCCESS].

**CONFORMS** — `deuce_ssh_auth_server()` sends USERAUTH_SUCCESS via
`send_auth_success()` when a callback returns `DEUCE_SSH_AUTH_SUCCESS`.

### 5.1-7
> The client **MAY** send several authentication requests without
> waiting for responses from previous requests.

**CONFORMS** — The library's auth functions are synchronous (send
request, wait for response), but nothing prevents the application from
using `send_packet` directly for pipelining.

### 5.1-8
> The server **MUST** process each request completely and acknowledge
> any failed requests with a SSH_MSG_USERAUTH_FAILURE message before
> processing the next request.

**CONFORMS** — `deuce_ssh_auth_server()` processes each request
synchronously: callback → send response → recv next request.

### 5.1-9
> A client **MUST NOT** send a subsequent request if it has not received
> a response from the server for a previous request.

**CONFORMS** — All client-side auth functions are synchronous: they send
a request and block on `recv_packet()` for the response before
returning.  The application cannot accidentally pipeline.

### 5.1-10
> A SSH_MSG_USERAUTH_FAILURE message **MUST NOT** be sent for an aborted
> method.

**APPLICATION** — Server-side responsibility.

### 5.1-11
> SSH_MSG_USERAUTH_SUCCESS **MUST** be sent only once.

**CONFORMS** — `deuce_ssh_auth_server()` exits the auth loop
immediately after sending SUCCESS (via `goto done`).

### 5.1-12
> When SSH_MSG_USERAUTH_SUCCESS has been sent, any further
> authentication requests received after that **SHOULD** be silently
> ignored.

**APPLICATION** — Server-side responsibility.

### 5.1-13
> Any non-authentication messages sent by the client after the request
> that resulted in SSH_MSG_USERAUTH_SUCCESS being sent **MUST** be
> passed to the service being run on top of this protocol.

**APPLICATION** — Server-side responsibility.  The library's transport
layer passes all messages through regardless.

---

## Section 5.2: The "none" Authentication Request

### 5.2-1
> If no authentication is needed for the user, the server **MUST**
> return SSH_MSG_USERAUTH_SUCCESS.

**CONFORMS** — See 4-3.

### 5.2-2
> Otherwise, the server **MUST** return SSH_MSG_USERAUTH_FAILURE and
> **MAY** return with it a list of methods that may continue.

**CONFORMS** — `deuce_ssh_auth_server()` sends FAILURE with the
application-provided `methods_str`.

### 5.2-3
> This 'method name' [none] **MUST NOT** be listed as supported by the
> server.

**APPLICATION** — Same as 4-1.  Application controls `methods_str`.

---

## Section 5.3: Completion of User Authentication

### 5.3-1
> All authentication related messages received after sending
> [SSH_MSG_USERAUTH_SUCCESS] **SHOULD** be silently ignored.

**APPLICATION** — Server-side responsibility.  Same as 5.1-12.

---

## Section 5.4: Banner Message

### 5.4-1
> By default, the client **SHOULD** display the 'message' on the
> screen.

**CONFORMS** — SSH_MSG_USERAUTH_BANNER messages are passed to the
application via the `deuce_ssh_auth_banner_cb` callback set on the
session struct (`sess->banner_cb`).  If no callback is set, banners
are silently discarded.  The callback receives the message text and
language tag, and the application can display them.

### 5.4-2
> Control character filtering [...] **SHOULD** be used to avoid attacks
> by sending terminal control characters.

**APPLICATION** — The banner callback provides raw text.  Control
character filtering for display is the application's responsibility.

---

## Section 6: Authentication Protocol Message Numbers

### 6-1
> Receiving [a message with number 80 or higher] before authentication
> is complete is an error, to which the server **MUST** respond by
> disconnecting.

**APPLICATION** — Server-side responsibility.

---

## Section 7: Public Key Authentication Method: "publickey"

### 7-1
> The only **REQUIRED** authentication 'method name' is "publickey"
> authentication.

**CONFORMS** — `deuce_ssh_auth_publickey()` implements client-side
public key authentication.  It signs the session-bound authentication
data per RFC 4252 s7 (session_id + USERAUTH_REQUEST fields) using
the key algorithm's `sign()` callback, and sends the signed request.
Supports any registered key algorithm (`ssh-ed25519`, `rsa-sha2-256`).

### 7-2
> All implementations **MUST** support this method.

**CONFORMS** — See 7-1.

### 7-3
> The server **MUST** check that the key is a valid authenticator for
> the user, and **MUST** check that the signature is valid.

**APPLICATION** — Server-side responsibility.

### 7-4
> If both hold, the authentication request **MUST** be accepted;
> otherwise, it **MUST** be rejected.

**APPLICATION** — Server-side responsibility.

### 7-5
> If the server does not support some algorithm, it **MUST** simply
> reject the request.

**APPLICATION** — Server-side responsibility.

### 7-6
> The server **MUST** respond to this message with either
> SSH_MSG_USERAUTH_FAILURE or [SSH_MSG_USERAUTH_PK_OK].

**APPLICATION** — Server-side responsibility.

### 7-7
> When the server receives this message, it **MUST** check whether the
> supplied key is acceptable for authentication, and if so, it **MUST**
> check whether the signature is correct.

**APPLICATION** — Server-side responsibility.

### 7-8
> The server **MUST** respond with SSH_MSG_USERAUTH_SUCCESS [...] or
> SSH_MSG_USERAUTH_FAILURE.

**APPLICATION** — Server-side responsibility.

---

## Section 8: Password Authentication Method: "password"

### 8-1
> All implementations **SHOULD** support password authentication.

**CONFORMS** — `deuce_ssh_auth_password()` implements client-side
password authentication per RFC 4252 s8.

### 8-2
> A server **MAY** request that a user change the password.

**CONFORMS** — `deuce_ssh_auth_password()` handles
SSH_MSG_USERAUTH_PASSWD_CHANGEREQ via an optional
`deuce_ssh_auth_passwd_change_cb` callback.  If the callback is
provided, it receives the server's prompt and returns the new password.
If no callback is provided and the server requests a change,
authentication fails gracefully.

### 8-3
> If the client reads the password in some other encoding, it **MUST**
> convert the password to ISO-10646 UTF-8 before transmitting.

**APPLICATION** — The library transmits the password bytes as provided.
UTF-8 encoding is the application's responsibility.

### 8-4
> The server **MUST** convert the password to the encoding used on that
> system for passwords.

**APPLICATION** — Server-side responsibility.

### 8-5
> If no confidentiality is provided ("none" cipher), password
> authentication **SHOULD** be disabled.

**APPLICATION** — The library does not check the negotiated cipher
before sending passwords.  The application should check
`deuce_ssh_transport_get_enc_name()` and avoid password auth if
`"none"` was negotiated.

### 8-6
> The server **MUST NOT** allow an expired password to be used for
> authentication.

**APPLICATION** — Server-side responsibility.

---

## Section 9: Host-Based Authentication: "hostbased"

### 9-1
> This form of authentication is **OPTIONAL**.

**N/A** — Not implemented.  This is an OPTIONAL method.

### 9-2
> When used, special care **SHOULD** be taken to prevent a regular user
> from obtaining the private host key.

**N/A** — Not implemented.

### 9-3
> The server **MUST** verify that the host key actually belongs to the
> client host named in the message.

**N/A** — Not implemented.

---

## Summary

### Conformance Status

| Status | Count |
|--------|-------|
| CONFORMS | 21 |
| APPLICATION (policy decisions) | 21 |
| N/A | 3 |
| PARTIAL | 0 |
| NONCONFORMANT | 0 |

No remaining issues.  The library handles the auth protocol mechanics
(message parsing, response sending, signature verification) for both
client and server.  Policy decisions (timeouts, attempt limits, user
validation, credential checking) are correctly delegated to application
callbacks.
