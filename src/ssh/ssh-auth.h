// RFC 4252: SSH Authentication Protocol

#ifndef DEUCE_SSH_AUTH_H
#define DEUCE_SSH_AUTH_H

#include "deucessh.h"

#define SSH_MSG_USERAUTH_REQUEST          UINT8_C(50)
#define SSH_MSG_USERAUTH_FAILURE          UINT8_C(51)
#define SSH_MSG_USERAUTH_SUCCESS          UINT8_C(52)
#define SSH_MSG_USERAUTH_BANNER           UINT8_C(53)

#define SSH_MSG_USERAUTH_INFO_REQUEST     UINT8_C(60)
#define SSH_MSG_USERAUTH_INFO_RESPONSE   UINT8_C(61)

/*
 * Request an SSH service (typically "ssh-userauth").
 * Sends SSH_MSG_SERVICE_REQUEST and waits for SERVICE_ACCEPT.
 * Returns 0 on success.
 */
int deuce_ssh_auth_request_service(deuce_ssh_session sess, const char *service);

/*
 * Query available authentication methods by sending a "none"
 * auth request.  On return:
 *   0 = no auth required (rare; methods buffer is empty)
 *   1 = auth required; methods is a comma-separated list
 *       (e.g., "publickey,password,keyboard-interactive")
 *   <0 = error
 */
int deuce_ssh_auth_get_methods(deuce_ssh_session sess,
    const char *username, char *methods, size_t methods_sz);

/*
 * Authenticate with a password (RFC 4252 s8).
 * Returns 0 on success, negative on failure or rejection.
 */
int deuce_ssh_auth_password(deuce_ssh_session sess,
    const char *username, const char *password);

/*
 * Authenticate via keyboard-interactive (RFC 4256).
 * Responds to all INFO_REQUEST prompts with the provided password.
 * Returns 0 on success, negative on failure or rejection.
 */
int deuce_ssh_auth_keyboard_interactive(deuce_ssh_session sess,
    const char *username, const char *password);

#endif
