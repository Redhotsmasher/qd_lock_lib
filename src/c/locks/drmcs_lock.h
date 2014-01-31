#ifndef DRMCS_LOCK_H
#define DRMCS_LOCK_H

#include "locks/oo_lock_interface.h"
#include "locks/mcs_lock.h"
#include "misc/padded_types.h"
#include "read_indicators/reader_groups_read_indicator.h"

#include "misc/bsd_stdatomic.h"//Until c11 stdatomic.h is available
#include "misc/thread_includes.h"//Until c11 thread.h is available
#include <stdbool.h>

#define DRMCS_READ_PATIENCE_LIMIT 130000

typedef struct {
    MCSLock lock;
    LLPaddedInt writeBarrier;
    ReaderGroupsReadIndicator readIndicator;
} DRMCSLock;

void drmcs_initialize(DRMCSLock * lock){
    mcs_initialize(&lock->lock);
    atomic_int tmp = ATOMIC_VAR_INIT(0);
    lock->writeBarrier.value = tmp;
    reader_groups_initialize(&lock->readIndicator);
}

void drmcs_lock(void * lock) {
    DRMCSLock *l = (DRMCSLock*)lock;
    while(atomic_load_explicit(&l->writeBarrier.value, memory_order_acquire)){
        thread_yield();
    }
    if(!mcs_lock_status(&l->lock)){
        rgri_wait_all_readers_gone(&l->readIndicator);
    }
}

void drmcs_unlock(void * lock) {
    DRMCSLock *l = (DRMCSLock*)lock;
    mcs_unlock(&l->lock);
}

bool drmcs_is_locked(void * lock){
    DRMCSLock *l = (DRMCSLock*)lock;
    return mcs_is_locked(&l->lock);
}

bool drmcs_try_lock(void * lock) {
    DRMCSLock *l = (DRMCSLock*)lock;
    while(atomic_load_explicit(&l->writeBarrier.value, memory_order_seq_cst) > 0){
        thread_yield();
    }
    if(mcs_try_lock(&l->lock)){
        rgri_wait_all_readers_gone(&l->readIndicator);
        return true;
    }else{
        return false;
    } 
}

void drmcs_rlock(void * lock) {
    DRMCSLock *l = (DRMCSLock*)lock;
    bool bRaised = false;
    int readPatience = 0;
 start:
    rgri_arrive(&l->readIndicator);
    if(mcs_is_locked(&l->lock)) {
        rgri_depart(&l->readIndicator);
        while(mcs_is_locked(&l->lock)) {
            thread_yield();
            if((readPatience == DRMCS_READ_PATIENCE_LIMIT) && !bRaised) {
                atomic_fetch_add_explicit(&l->writeBarrier.value, 1, memory_order_seq_cst);
                bRaised = true;
            }
            readPatience = readPatience + 1;
        }
        goto start;
    }
    if(bRaised) {
        atomic_fetch_sub_explicit(&l->writeBarrier.value, 1, memory_order_seq_cst);
    }
}

void drmcs_runlock(void * lock) {
    DRMCSLock *l = (DRMCSLock*)lock;
    rgri_depart(&l->readIndicator);
}

void drmcs_delegate(void * lock,
                    void (*funPtr)(unsigned int, void *), 
                    unsigned int messageSize,
                    void * messageAddress){
    DRMCSLock *l = (DRMCSLock*)lock;
    drmcs_lock(l);
    funPtr(messageSize, messageAddress);
    drmcs_unlock(l);
}

void * drmcs_delegate_or_lock(void * lock, unsigned int messageSize){
    (void)messageSize;
    DRMCSLock *l = (DRMCSLock*)lock;
    drmcs_lock(l);
    return NULL;
}

_Alignas(CACHE_LINE_SIZE)
OOLockMethodTable DRMCS_LOCK_METHOD_TABLE = 
{
     .free = &free,
     .lock = &drmcs_lock,
     .unlock = &drmcs_unlock,
     .is_locked = &drmcs_is_locked,
     .try_lock = &drmcs_try_lock,
     .rlock = &drmcs_rlock,
     .runlock = &drmcs_runlock,
     .delegate = &drmcs_delegate,
     .delegate_wait = &drmcs_delegate,
     .delegate_or_lock = &drmcs_delegate_or_lock,
     .close_delegate_buffer = NULL, /* Should never be called */
     .delegate_unlock = &drmcs_unlock
};

DRMCSLock * plain_drmcs_create(){
    DRMCSLock * l = aligned_alloc(CACHE_LINE_SIZE, sizeof(DRMCSLock));
    drmcs_initialize(l);
    return l;
}

OOLock * oo_drmcs_create(){
    DRMCSLock * l = plain_drmcs_create();
    OOLock * ool = aligned_alloc(CACHE_LINE_SIZE, sizeof(OOLock));
    ool->lock = l;
    ool->m = &DRMCS_LOCK_METHOD_TABLE;
    return ool;
}

#endif
