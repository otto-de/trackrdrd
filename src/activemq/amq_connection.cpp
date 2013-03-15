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

#include "amq_connection.h"
#include <decaf/lang/exceptions/NullPointerException.h>

#define CATCHALL                \
catch (CMSException& cex) {     \
    return cex.what();          \
 }                              \
catch (Throwable& th) {         \
    return th.what();           \
 }                              \
catch (std::exception& sex) {   \
    return sex.what();          \
 }                              \
catch (...) {                   \
     return "Unexpected error"; \
 }

using namespace std;
using namespace activemq::core;
using namespace cms;
using namespace decaf::lang;
using namespace decaf::lang::exceptions;

ActiveMQConnectionFactory* AMQ_Connection::factory = NULL;

AMQ_Connection::AMQ_Connection(std::string& brokerURI) {

    if (brokerURI.length() == 0)
        throw IllegalArgumentException(__FILE__, __LINE__,
                                       "Broker URI is empty");
    
    factory = new ActiveMQConnectionFactory(brokerURI);
    if (factory == NULL)
        throw NullPointerException(__FILE__, __LINE__,
                                   "Factory created for %s is NULL",
                                   brokerURI.c_str());
       
    connection = factory->createConnection();
    if (connection == NULL)
        throw NullPointerException(__FILE__, __LINE__,
                                   "Connection created for %s is NULL",
                                   brokerURI.c_str());
    connection->start();
}

Connection *
AMQ_Connection::getConnection() {
    return connection;
}

AMQ_Connection::~AMQ_Connection() {
    if (connection != NULL) {
        connection->stop();
        connection->close();
	delete connection;
	connection = NULL;
    }
}

const char *
AMQ_ConnectionInit(AMQ_Connection **cn, char *uri)
{
    try {
        string brokerURI (uri);
        auto_ptr<AMQ_Connection> c (new AMQ_Connection(brokerURI));
        *cn = c.release();
        return NULL;
    }
    CATCHALL
}

const char *
AMQ_ConnectionShutdown(AMQ_Connection *cn)
{
    try {
        delete cn;
        return NULL;
    }
    CATCHALL
}
