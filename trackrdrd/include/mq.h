/*-
 * Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Author: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * \file mq.h
 * \brief MQ messaging interface for trackrdrd
 * \details MQ -- the messaging interface for the Varnish log tracking
 * reader
 * \version 3
 *
 * This header defines the interface to a messaging system, such as
 * ActiveMQ or Kafka, used by the tracking reader. It is responsible for
 * implementing connections to the messaging system, sending data,
 * detecting errors and managing resources.
 *
 * An implementation of this interface is a dynamic library (shared
 * object) that must provide definitions for each of the functions
 * declared here (read by the tracking reader via dlsym(3)).
 *
 * The tracking reader starts a configurable number of worker threads that
 * are responsible for sending data to a messaging system, by calling the
 * MQ_Send() method. The messaging implementation is given the opportunity
 * to create and use a per-thread private object for each worker thread,
 * declared in the following as `void *priv` and initialized by
 * MQ_WorkerInit().  A thread-safe implementation must be provided for
 * each operation defined with such an object as an argument.
 *
 * With the exception of MQ_Send(), each operation in this interface is
 * expected to return `NULL` on success, or an error string on failure, to
 * be used by the tracking reader to log error messages. MQ_Send() is
 * expected to return 0 for a successful send, an integer greater than 0
 * for a recoverable error, or an integer less than 0 for a
 * non-recoverable error; and to place an error message in its `error`
 * parameter. Non-recoverable errors should be signaled when internal
 * structures of the messaging implementation must be shut down and
 * re-initialized, for example when a network connection has been lost or
 * has become unreliable; in this case, the tracking reader performs the
 * error recovery procedure described below. After recoverable errors from
 * MQ_Send(), the tracking reader simply logs the error message and
 * continues.
 *
 * The pointers to error messages returned from operations in this
 * interface, or set in the `error` parameter of MQ_Send(), should point
 * to static storage. The tracking reader does not attempt to free
 * non-`NULL` pointers returned from the interface.
 *
 * The methods in this interface are called in the following order:
 *
 * - MQ_GlobalInit() is called when the tracking reader initializes,
 *   before any other methods. If it fails, then the tracking reader
 *   fails.
 * - MQ_InitConnections() is called after successful return of
 *   MQ_GlobalInit(), before any worker threads are created. It is
 *   intended for the initialization of network connections; the tracking
 *   reader fails (and does not bother to start any threads) if this
 *   method fails.
 *
 * In each worker thread:
 *
 * - MQ_WorkerInit() is called when the thread initializes; the thread
 *   fails if this method fails.
 * - If MQ_WorkerInit() succeeds, then MQ_Version() and MQ_ClientID()
 *   are called at initialization (so that the version and client ID
 *   can be written to the log). If either of them fail, an error is
 *   logged, but the thread continues.
 * - The main loop of the worker thread calls MQ_Send() for every data
 *   record that it processes. See below for a description of how the
 *   tracking reader handles non-recoverable message send failures.
 * - MQ_WorkerShutdown() is called when the worker thread is shutting
 *   down.
 *
 * MQ_GlobalShutdown() is called when the tracking reader worker process
 * (child process) is shutting down. If the call fails, the error
 * message is logged and the process shutdown continues.
 *
 * Once a worker thread has entered its main loop (and hence global
 * initialization, initialization of network connections and of a private
 * worker object have succeeded), the tracking reader handles
 * non-recoverable failures of message sends as follows:
 *
 * - If MQ_Send() fails, the thread calls MQ_Reconnect(); the messaging
 *   implementation is expected to attempt a new connection, and may
 *   create a new private worker object. If MQ_Reconnect() succeeds,
 *   then MQ_Send() is attempted again with the same data.
 * - If either MQ_Reconnect() fails, or the resend after a successful call
 *   to MQ_Reconnect() fails, then the private worker object is discarded
 *   (with a call to MQ_WorkerShutdown()), and the worker thread
 *   stops. The tracking reader may attempt to start a new thread in its
 *   place, in which case a new private worker object for the messaging
 *   implementation is initialized.
 */

/**
 * Global initialization of the messaging implementation
 *
 * @param nworkers the number of worker threads
 * @param config_fname path of a configuration file specific to the
 * messaging implementation
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_GlobalInit(unsigned nworkers, const char *config_fname);

/**
 * Initialize network connections to the messaging system
 *
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_InitConnections(void);

/**
 * Initialize a private object used by one of the tracking reader's worker
 * threads.
 *
 * The implementation of this method must be thread-safe.
 *
 * @param priv pointer to a private object handle. The implementation is
 * expected to place a pointer to its private data structure in this
 * location.
 * @param wrk_num the worker number, from 1 to the value of ``nworkers``
 * supplied in ``MQ_GlobalInit()``, inclusive
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_WorkerInit(void **priv, int wrk_num);

/**
 * Send data to the messaging system.
 *
 * On failure, the implementation must signal whether the error is
 * recoverable or non-recoverable, with a return code greater than or less
 * than zero, respectively. If the implementation can safely continue with
 * the state referenced by its private object handle, the error is
 * recoverable; if its private structures must be shut down / destroyed,
 * the error is non-recoverable (and the tracking reader initiates the
 * shutdown and possible re-initialization as described above).
 *
 * The implementation of this method must be thread-safe.
 *
 * @param priv private object handle
 * @param data pointer to the data to be sent
 * @param len length of the data in bytes
 * @param key an optional sharding key for the messaging system
 * @param keylen length of the sharding key
 * @param error pointer to an error message. The implementation is
 * expected to place a message in this location when non-zero is returned.
 * @return zero on success, >0 for a recoverable error, <0 for a
 * non-recoverable error
 */
int MQ_Send(void *priv, const char *data, unsigned len,
            const char *key, unsigned keylen, const char **error);

/**
 * Return the version string of the messaging system.
 *
 * The implementation of this method must be thread-safe.
 *
 * The tracking reader does not attempt to free the address returned in
 * `version`.
 *
 * @param priv private object handle
 * @param version pointer to the version string. The implementation is
 * expected to place the starting address of a null-terminated string in
 * this location.
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_Version(void *priv, char *version);

/**
 * Return an ID string for the client connection.
 *
 * The implementation of this method must be thread-safe.
 *
 * The tracking reader does not attempt to free the address returned in
 * `clientID`.
 *
 * @param priv private object handle
 * @param clientID pointer to the client ID string. The implementation is
 * expected to place the starting address of a null-terminated string in
 * this location.
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_ClientID(void *priv, char *clientID);

/**
 * Re-initialize a connection to the messaging system after a send
 * failure.
 *
 * The implementation of this method must be thread-safe.
 *
 * The implementation is responsible for disconnecting the existing
 * connection, if it so chooses, and may initialize a new private object;
 * the implementation is responsible for cleaning up resources as
 * necessary.
 *
 * @param priv pointer to the private object handle. If a new object is
 * created, the implementation is expected to place its address in this
 * location.
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_Reconnect(void **priv);

/**
 * Shut down message processing for a worker thread.
 *
 * The implementation of this method must be thread-safe.
 *
 * The implementation is responsible for cleaning up resources as
 * necessary. The tracking reader does not access `priv` after calling
 * this method (so it may, for example, be set to `NULL`).
 *
 * @param priv pointer to the private object handle
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_WorkerShutdown(void **priv);

/**
 * Globally shut down the messaging implementation
 *
 * The implementation is responsible for final cleanup of resources as
 * necessary.
 *
 * @return `NULL` on success, an error message on failure
 */
const char *MQ_GlobalShutdown(void);
