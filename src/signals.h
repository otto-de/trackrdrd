/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
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

PARENT(SIGTERM, terminate_action);
PARENT(SIGINT, terminate_action);
PARENT(SIGHUP, restart_action);
PARENT(SIGUSR1, restart_action);
PARENT(SIGUSR2, ignore_action);
#ifndef DISABLE_STACKTRACE
PARENT(SIGABRT, stacktrace_action);
PARENT(SIGSEGV, stacktrace_action);
PARENT(SIGBUS, stacktrace_action);
#endif

CHILD(SIGTERM, terminate_action);
CHILD(SIGINT, terminate_action);
CHILD(SIGUSR1, dump_action);
CHILD(SIGUSR2, ignore_action);
CHILD(SIGHUP, ignore_action);
#ifndef DISABLE_STACKTRACE
CHILD(SIGABRT, stacktrace_action);
CHILD(SIGSEGV, stacktrace_action);
CHILD(SIGBUS, stacktrace_action);
#endif
