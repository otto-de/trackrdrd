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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "minunit.h"

#include "../trackrdrd.h"

#define DEBUG 0
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while(0)

#define NCON 10

#define TABLE_SIZE ((1 << DEF_MAXOPEN_SCALE) + DEF_MAXDONE)

int run;

typedef enum {
    SUCCESS = 0,
    PRODUCER_QFULL,
    PRODUCER_BCAST,
    CONSUMER_MUTEX,
    CONSUMER_WAIT,
    CONSUMER_BCAST
} fail_e;

typedef struct {
    unsigned sum;
    fail_e fail;
} prod_con_data_t;

int tests_run = 0;
static char errmsg[BUFSIZ];

static unsigned xids[TABLE_SIZE];
static prod_con_data_t proddata;
static prod_con_data_t condata[NCON];

static void
*producer(void *arg)
{
    //prod_con_data_t pcdata;
    proddata.sum = 0;
    proddata.fail = SUCCESS;
    int enqs = 0;

    (void) arg;

    srand48(time(NULL));
    unsigned xid = (unsigned int) lrand48();

    for (int i = 0; i < (1 << DEF_MAXOPEN_SCALE); i++) {
        xids[i] = xid;
        debug_print("Producer: enqueue %d (xid = %u)\n", ++enqs, xid);
        if (!SPMCQ_Enq(&xids[i])) {
            proddata.fail = PRODUCER_QFULL;
            pthread_exit(&proddata);
        }
        debug_print("%s\n", "Producer: broadcast");
        if (pthread_cond_broadcast(&spmcq_datawaiter_cond) != 0) {
            proddata.fail = PRODUCER_BCAST;
            pthread_exit(&proddata);
        }
        proddata.sum += xid;
        xid++;
    }
    debug_print("%s\n", "Producer: exit");
    pthread_exit((void *) &proddata);
}

#define consumer_exit(pcdata, reason)	\
    do {				\
        (pcdata)->fail = (reason);	\
        pthread_exit((pcdata));		\
    } while(0)

static void
*consumer(void *arg)
{
    int id = *((int *) arg), deqs = 0;
    prod_con_data_t *pcdata = &condata[id-1];
    pcdata->sum = 0;
    pcdata->fail = SUCCESS;
    unsigned *xid;

    while (run) {
	/* run may be stale at this point */
        debug_print("Consumer %d: attempt dequeue\n", id);
	xid = (unsigned *) SPMCQ_Deq();
	if (xid == NULL) {
	    /* grab the CV lock, which also constitutes an implicit memory
               barrier */
	    debug_print("Consumer %d: mutex\n", id);
	    if (pthread_mutex_lock(&spmcq_datawaiter_lock) != 0)
		consumer_exit(pcdata, CONSUMER_MUTEX);
	    /* run is guaranteed to be fresh here */
	    if (run) {
		debug_print("Consumer %d: wait, run = %d\n", id, run);
		if (pthread_cond_wait(&spmcq_datawaiter_cond,
                                      &spmcq_datawaiter_lock) != 0)
		    consumer_exit(pcdata, CONSUMER_WAIT);
	    }
	    debug_print("Consumer %d: unlock\n", id);
	    if (pthread_mutex_unlock(&spmcq_datawaiter_lock) != 0)
		consumer_exit(pcdata, CONSUMER_MUTEX);
	    if (! run) {
		debug_print("Consumer %d: quit signaled, run = %d\n", id, run);
		break;
	    }
	} else {
	    /* xid != NULL */
	    debug_print("Consumer %d: dequeue %d (xid = %u)\n", id, ++deqs,
                *xid);
            pcdata->sum += *xid;
        }
    }
    debug_print("Consumer %d: drain queue, run = %d\n", id, run);
    while ((xid = (unsigned *) SPMCQ_Deq()) != NULL) {
        debug_print("Consumer %d: dequeue %d (xid = %u)\n", id, ++deqs, *xid);
        pcdata->sum += *xid;
    }
    debug_print("Consumer %d: exit\n", id);
    pthread_exit((void *) pcdata);
}

/* N.B.: Always run this test first */
static char
*test_spmcq_init(void)
{
    int err;

    printf("... testing SPMCQ initialization\n");

    if (pthread_mutex_init(&spmcq_datawaiter_lock, NULL) != 0) {
        sprintf(errmsg, "mutex_init failed: %s", strerror(errno));
        return(errmsg);
    }
    if (pthread_cond_init(&spmcq_datawaiter_cond, NULL) != 0) {
        sprintf(errmsg, "cond_init failed: %s", strerror(errno));
        return(errmsg);
    }
    
    config.maxopen_scale = DEF_MAXOPEN_SCALE;
    config.maxdone = DEF_MAXDONE;
    err = SPMCQ_Init();
    sprintf(errmsg, "SPMCQ_Init: %s", strerror(err));
    mu_assert(errmsg, err == 0);

    return NULL;
}

static const char
*test_spmcq_enq_deq(void)
{
    bool r;
    unsigned xid = 1234567890, *xid2;

    printf("... testing SPMCQ enqueue and dequeue\n");
    
    r = SPMCQ_Enq(&xid);
    mu_assert("SPMCQ_Enq failed", r);

    xid2 = SPMCQ_Deq();
    sprintf(errmsg, "SMPCQ_Deq: expected %d, got %d", xid, *xid2);
    mu_assert(errmsg, xid == *xid2);
    
    return NULL;
}

static const char
*test_spmcq_twocon(void)
{
    pthread_t prod, con1, con2;
    prod_con_data_t *prod_data, *con1_data, *con2_data;
    int err, id1, id2;
    
    printf("... testing multithreaded SPMCQ with two consumers\n");

    run = 1;
    err = pthread_create(&prod, NULL, producer, NULL);
    sprintf(errmsg, "Failed to create producer: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    id1 = 1;
    err = pthread_create(&con1, NULL, consumer, &id1);
    sprintf(errmsg, "Failed to create consumer 1: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    id2 = 2;
    err = pthread_create(&con2, NULL, consumer, &id2) ;
    sprintf(errmsg, "Failed to create consumer 2: %s", strerror(err));
    mu_assert(errmsg, err == 0);

    err = pthread_join(prod, (void **) &prod_data);
    sprintf(errmsg, "Failed to join producer: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    
    /*
     * must only modify run under spmcq_datawaiter_lock to ensure that
     * we signal all waiting consumers (otherwise a consumer could go
     * waiting _after_ we have broadcasted and so miss the event.
     */
    MAZ(pthread_mutex_lock(&spmcq_datawaiter_lock));
    run = 0;
    MAZ(pthread_cond_broadcast(&spmcq_datawaiter_cond));
    MAZ(pthread_mutex_unlock(&spmcq_datawaiter_lock));
    
    err = pthread_join(con1, (void **) &con1_data);
    sprintf(errmsg, "Failed to join consumer 1: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    err = pthread_join(con2, (void **) &con2_data);
    sprintf(errmsg, "Failed to join consumer 2: %s", strerror(err));
    mu_assert(errmsg, err == 0);

    if (prod_data->fail != SUCCESS) {
        if (prod_data->fail == PRODUCER_QFULL)
            sprintf(errmsg, "Producer: queue full");
        else if (prod_data->fail == PRODUCER_BCAST)
            sprintf(errmsg, "Producer: broadcast failed");
        mu_assert(errmsg, prod_data->fail == SUCCESS);
    }

    if (con1_data->fail != SUCCESS) {
        if (con1_data->fail == CONSUMER_MUTEX)
            sprintf(errmsg, "Consumer 1: mutex failed");
        else if (con1_data->fail == CONSUMER_WAIT)
            sprintf(errmsg, "Consumer 1: conditional wait failed");
        else if (con1_data->fail == CONSUMER_BCAST)
            sprintf(errmsg, "Consumer 1: broadcast failed");
        mu_assert(errmsg, con1_data->fail == SUCCESS);
    }

    if (con2_data->fail != SUCCESS) {
        if (con2_data->fail == CONSUMER_MUTEX)
            sprintf(errmsg, "Consumer 2: mutex failed");
        else if (con2_data->fail == CONSUMER_WAIT)
            sprintf(errmsg, "Consumer 2: conditional wait failed");
        else if (con2_data->fail == CONSUMER_BCAST)
            sprintf(errmsg, "Consumer 2: broadcast failed");
        mu_assert(errmsg, con2_data->fail == SUCCESS);
    }

    sprintf(errmsg, "Consumer/producer checksum mismatch: p = %u, c = %u",
        prod_data->sum, con1_data->sum + con2_data->sum);
    mu_assert(errmsg, prod_data->sum == con1_data->sum + con2_data->sum);
    
    return NULL;
}

static const char
*test_spmcq_manycon(void)
{
    pthread_t prod, con[NCON];
    prod_con_data_t *prod_data, *con_data[NCON];
    fail_e prod_fail;
    int err, id[NCON];
    unsigned prodsum, consum = 0;
    
    printf("... testing multithreaded SPMCQ with %d consumers\n", NCON);

    run = 1;
    err = pthread_create(&prod, NULL, producer, NULL);
    sprintf(errmsg, "Failed to create producer: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    
    for (int i = 0; i < NCON; i++) {
        id[i] = i + 1;
        err = pthread_create(&con[i], NULL, consumer, &id[i]);
        sprintf(errmsg, "Failed to create consumer %d: %s", i+1, strerror(err));
        mu_assert(errmsg, err == 0);
    }

    err = pthread_join(prod, (void **) &prod_data);
    sprintf(errmsg, "Failed to join producer: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    prod_fail = prod_data->fail;
    prodsum = prod_data->sum;
    
    /*
     * must only modify run under spmcq_datawaiter_lock to ensure that
     * we signal all waiting consumers (otherwise a consumer could go
     * waiting _after_ we have broadcasted and so miss the event.
     */
    MAZ(pthread_mutex_lock(&spmcq_datawaiter_lock));
    run = 0;
    MAZ(pthread_cond_broadcast(&spmcq_datawaiter_cond));
    MAZ(pthread_mutex_unlock(&spmcq_datawaiter_lock));

    for (int i = 0; i < NCON; i++) {
        err = pthread_join(con[i], (void **) &con_data[i]);
        sprintf(errmsg, "Failed to join consumer %d: %s", i+1, strerror(err));
        mu_assert(errmsg, err == 0);
    }
    
    if (prod_fail != SUCCESS) {
        if (prod_fail == PRODUCER_QFULL)
            sprintf(errmsg, "Producer: queue full");
        else if (prod_fail == PRODUCER_BCAST)
            sprintf(errmsg, "Producer: broadcast failed");
        else
            sprintf(errmsg, "Producer: unknown error %d", prod_fail);
        mu_assert(errmsg, prod_fail == SUCCESS);
    }

    for (int i = 0; i < NCON; i++)
        if (con_data[i]->fail != SUCCESS) {
            if (con_data[i]->fail == CONSUMER_MUTEX)
                sprintf(errmsg, "Consumer %d: mutex failed", i+1);
            else if (con_data[i]->fail == CONSUMER_WAIT)
                sprintf(errmsg, "Consumer %d: conditional wait failed", i+1);
            else if (con_data[i]->fail == CONSUMER_BCAST)
                sprintf(errmsg, "Consumer %d: broadcast failed", i+1);
            else
                sprintf(errmsg, "Consumer %d: unknown error %d", i+1,
                    con_data[i]->fail);
            mu_assert(errmsg, con_data[i]->fail == SUCCESS);
        }

    for (int i = 0; i < NCON; i++)
        consum += con_data[i]->sum;
    
    sprintf(errmsg, "Consumer/producer checksum mismatch: p = %u, c = %u",
        prodsum, consum);
    mu_assert(errmsg, prodsum == consum);
    
    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_spmcq_init);
    mu_run_test(test_spmcq_enq_deq);
    mu_run_test(test_spmcq_twocon);
    mu_run_test(test_spmcq_manycon);
    return NULL;
}

TEST_RUNNER
