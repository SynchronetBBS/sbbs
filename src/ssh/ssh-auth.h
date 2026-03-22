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

int deuce_ssh_auth_request_service(deuce_ssh_session sess, const char *service);
int deuce_ssh_auth_get_methods(deuce_ssh_session sess,
    const char *username, char *methods, size_t methods_sz);
int deuce_ssh_auth_password(deuce_ssh_session sess,
    const char *username, const char *password);
int deuce_ssh_auth_keyboard_interactive(deuce_ssh_session sess,
    const char *username, const char *password);

#endif
