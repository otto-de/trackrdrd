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

#include <string.h>

#include "amq.h"
#include <activemq/library/ActiveMQCPP.h>
#include <activemq/core/ActiveMQConnection.h>
#include <decaf/lang/exceptions/NullPointerException.h>
#include <cms/IllegalStateException.h>
#include <cms/ConnectionMetaData.h>

#define CATCHALL                \
catch (CMSException& cex) {     \
    return cex.what();          \
 }                              \
catch (Throwable& th) {         \
    return th.what();           \
 }                              \
 catch (std::exception& sex) {  \
    return sex.what();          \
 }                              \
 catch (...) {                  \
     return "Unexpected error"; \
 }

using namespace std;
using namespace activemq::core;
using namespace cms;
using namespace decaf::lang;
using namespace decaf::lang::exceptions;

AMQ_Worker::AMQ_Worker(Connection* cn, std::string& qName, int n,
    Session::AcknowledgeMode ackMode = Session::AUTO_ACKNOWLEDGE,
    int deliveryMode = DeliveryMode::NON_PERSISTENT) {

    connection = cn;
    num = n;
    session = connection->createSession(ackMode);
    queue = session->createQueue(qName);
    producer = session->createProducer(queue);
    producer->setDeliveryMode(deliveryMode);
    msg = session->createTextMessage();
}

AMQ_Worker::~AMQ_Worker() {
    if (queue != NULL) {
	delete queue;
	queue = NULL;
    }
    if (producer != NULL) {
	delete producer;
	producer = NULL;
    }
    if (session != NULL) {
        session->stop();
	delete session;
	session = NULL;
    }
}

int
AMQ_Worker::getNum() {
    return num;
}

void
AMQ_Worker::send(std::string& text) {
    if (msg == NULL || producer == NULL)
        throw cms::IllegalStateException("Worker fields are NULL");
    msg->setText(text);
    producer->send(msg);
}

std::string
AMQ_Worker::getVersion() {
    if (connection == NULL)
        throw cms::IllegalStateException("Connection uninitialized");
    const ConnectionMetaData *md = connection->getMetaData();
    return md->getCMSProviderName() + " " + md->getProviderVersion();
}

std::string
AMQ_Worker::getClientID() {
    if (connection == NULL)
        throw cms::IllegalStateException("Connection uninitialized");
    return connection->getClientID();
}

const char *
AMQ_GlobalInit(void)
{
    try {
        activemq::library::ActiveMQCPP::initializeLibrary();
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_WorkerInit(AMQ_Worker **worker, AMQ_Connection *cn, char *qName, int n)
{
    try {
        Connection *conn = cn->getConnection();
        string queueName (qName);
        std::auto_ptr<AMQ_Worker> w (new AMQ_Worker(conn, queueName, n));
        *worker = w.release();
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_Send(AMQ_Worker *worker, const char *data, unsigned len)
{
    try {
        if (worker == NULL)
            throw NullPointerException(__FILE__, __LINE__, "AMQ_Worker NULL");
        if (data == NULL)
            throw IllegalArgumentException(__FILE__, __LINE__, "Data NULL");
        string text (data, len);
        worker->send(text);
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_Version(AMQ_Worker *worker, char *version)
{
    try {
        strcpy(version, worker->getVersion().c_str());
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_ClientID(AMQ_Worker *worker, char *clientID)
{
    try {
        strcpy(clientID, worker->getClientID().c_str());
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_GetNum(AMQ_Worker *worker, int *n)
{
    try {
        *n = worker->getNum();
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_WorkerShutdown(AMQ_Worker **worker)
{
    try {
        delete *worker;
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_GlobalShutdown()
{
    try {
        activemq::library::ActiveMQCPP::shutdownLibrary();
        return NULL;
    }
    CATCHALL
}
