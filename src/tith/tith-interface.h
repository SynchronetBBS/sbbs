#ifndef TITH_INTERFACE_HEADER
#define TITH_INTERFACE_HEADER

#include <stdbool.h>
#include <stdint.h>

/*
 * This header describes the interface that the TITH mailer uses
 * 
 * This can be implemented using platform-specific code.
 * See tith-stdio.c for an example implementation.
 */

/*
 * This should wait for and return a single byte from the connection.
 * 
 * The return value should be the value of the byte cast to a uint8_t.
 * -1 should be returned if an error occured and the connection is no
 * longer able to be read.
 */
int getByte(void *handle);

/*
 * This should wait for and return the specified number of bytes from
 * the connection.
 * 
 * It should return true if the requested number of bytes were read, and
 * false if an error occured and the connection is no longer able to be
 * read.
 */
bool getBytes(void *handle, uint8_t *buf, size_t bufsz);

/*
 * This is called when no more bytes will be retreived from a
 * connection. This should cause getChar() and getBytes() to return -1,
 * but not impact sendChar() or sendBytes().
 */
void shutdownRead(void *handle);

/*
 * This should send or buffer a single byte to the connection and block
 * until it is sent/buffered.
 * 
 * It should return true if the bytes was sent/buffered, and false if an
 * error occured and the connection is no longer able to be written to.
 */
bool sendByte(void *handle, uint8_t ch);

/*
 * This should send/buffer the entire buffer to the connection, blocking
 * until it is sent/buffered.
 * 
 * It should return true if the requested number of bytes were
 * sent/buffered, and false if an error occured and the connection is no
 * longer able to be written to.
 */
bool sendBytes(void *handle, uint8_t *buf, size_t bufsz);

/*
 * If bytes were buffered instead of sent by sendChar() and sendBytes(),
 * this causes them to be sent.
 * 
 * Returns false if an error occured and the connection is no longer
 * able to be written to.
 */
bool flushWrite(void *handle);

/*
 * This is called when no more bytes will be sent on a connection. This
 * should cause sendChar() and sendBytes() to return -1, but not impact
 * getChar() or getBytes().
 */
void shutdownWrite(void *handle);

/*
 * This indicates that all resources associated with the connection
 * should be freed, and the handle will not be used again.
 */
void closeConnection(void *handle);

/*
 * This logs a string.
 */
void logString(const char *str);

/*
 * This is the entry point to the TITH code.
 * 
 * handle is a connection to the remote, and if client is set, this
 * process initiated the connection.
 */
int TITH_main(int argc, char **argv, void *handle);

#endif
