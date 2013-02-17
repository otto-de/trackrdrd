/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
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

#ifndef _AMQ_H
#define _AMQ_H

#ifdef __cplusplus

#include <vector>

#include <activemq/core/ActiveMQConnectionFactory.h>
#include <cms/Connection.h>
#include <cms/Session.h>
#include <cms/Queue.h>
#include <cms/MessageProducer.h>

using namespace activemq::core;
using namespace cms;

class AMQ_Worker {
private:
    static std::vector<ActiveMQConnectionFactory*> factories;
    Connection* connection;
    Session* session;
    Queue* queue;
    MessageProducer* producer;
    TextMessage* msg;
    AMQ_Worker() {};

public:
    static void shutdown();
    
    AMQ_Worker(std::string& brokerURI, std::string& qName,
        Session::AcknowledgeMode ackMode, int deliveryMode);
    virtual ~AMQ_Worker();
    void send(std::string& text);
    std::string getVersion();
};
#else
typedef struct AMQ_Worker AMQ_Worker;
#endif
    
#ifdef __cplusplus
extern "C" {
#endif

    const char *AMQ_GlobalInit(void);
    const char *AMQ_WorkerInit(AMQ_Worker **worker, char *uri, char *qName);
    const char *AMQ_Send(AMQ_Worker *worker, const char *data, unsigned len);
    const char *AMQ_Version(AMQ_Worker *worker, char *version);
    const char *AMQ_WorkerShutdown(AMQ_Worker **worker);
    const char *AMQ_GlobalShutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* _AMQ_H */
