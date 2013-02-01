#include "gc.h"


/*****************************************************************************
    INTERNALS
 *****************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>


//include files for locking
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif


//multithreaded
#if GC_MULTITHREADED == 1

//win32 locking
#ifdef WIN32
static CRITICAL_SECTION cr;
static void initLock() { InitializeCriticalSection(&cr); }
static void deleteLock() { DeleteCriticalSection(&cr); }
static void lock() { EnterCriticalSection(&cr); }
static void unlock() { LeaveCriticalSection(&cr); }
#else
// POSIX platforms
static pthread_mutex_t cr;
static int cr_alreadyLocked = 0;

static void initLock() { pthread_mutex_init(&cr, NULL); }
static void deleteLock() { pthread_mutex_destroy(&cr); }

static void lock() {
    if(!cr_alreadyLocked) {
        pthread_mutex_lock(&cr);
        cr_alreadyLocked = 1;
    }
}

static void unlock() {
    cr_alreadyLocked = 0;
    pthread_mutex_unlock(&cr);
}
#endif

//else single-threaded
#else // WIN32
//empty functions that will be removed by the compiler
static void initLock() {}
static void deleteLock() {}
static void lock() {}
static void unlock() {}
#endif //GC_MULTITHREADED


namespace gc {


//max blocks
#define _MAX_BLOCKS          262144


//max roots
#define _MAX_ROOTS           262144


//internal object for doing initialization and clean up
struct __library {
    //dynamic initialization
    __library();

    //clean up
    ~__library();
};


//memory block descriptor
struct _block {
    Object *object;
    Object *new_object;
    size_t ptrs;
    size_t size:28;
    size_t mark_phase:1;
    size_t adjust_phase:1;
    size_t locked:1;
    size_t deleted:1;
};


//root pointer descriptor
struct _root {
    size_t prev;
    size_t next;
    _basic_ptr *ptr;
};


//context
static size_t _phase = 0;
static char _memory[GC_MEMORY_SIZE];
static size_t _alloc_size = 0;
static size_t _free_index = 0;
static _block _blocks[_MAX_BLOCKS];
static size_t _curr_block = 0;
static _root _roots[_MAX_ROOTS];
static size_t _root_free = 1;
static size_t _root_deleted = 0;


//mark object reachable from pointer
static void _mark(_basic_ptr *p)
{
    //pointer is null
    if (!p->object) return;

    //get block
    _block *block = &_blocks[*((size_t *)p->object - 1)];

    //do nothing for locked blocks or for already marked blocks
    if (block->locked || block->mark_phase == _phase) return;

    //mark object
    block->mark_phase = _phase;

    //mark blocks reachable from pointers of object
    size_t bp = block->ptrs;
    while (bp) {
        _basic_ptr *ptr = (_basic_ptr *)((char *)block->object + bp);
        _mark(ptr);
        bp = ptr->index;
    }
}


//adjust a pointer
static void _adjust(_basic_ptr *p)
{
    //pointer is null
    if (!p->object) return;

    //get block
    _block *block = &_blocks[*((size_t *)p->object - 1)];

    //do nothing for locked blocks
    if (block->locked) return;

    //adjust pointer
    p->object = block->new_object;

    //adjust pointers of block
    if (block->adjust_phase == _phase) return;
    block->adjust_phase = _phase;
    size_t bp = block->ptrs;
    while (bp) {
        _basic_ptr *ptr = (_basic_ptr *)((char *)block->object + bp);
        _adjust(ptr);
        bp = ptr->index;
    }
}


//collect garbage
static size_t _collect()
{
    //next phase
    _phase ^= 1;

    //mark blocks reachable from the root set
    size_t root_p = _roots[_root_free].prev;
    while (root_p) {
        _basic_ptr *ptr = _roots[root_p].ptr;
        _mark(ptr);
        root_p = _roots[root_p].prev;
    }

    //process objects
    size_t i, new_curr_block = 0, new_alloc_size = 0;
    _free_index = 0;
    for(i = 0; i < _curr_block; ++i) {
        //if block is locked, do nothing
        if (_blocks[i].locked) {
            if (!_blocks[i].deleted) {
                *((size_t *)_blocks[i].object - 1) = new_curr_block;
                _blocks[new_curr_block] = _blocks[i];
                new_alloc_size += _blocks[i].size + sizeof(size_t);
                ++new_curr_block;
            }
        }

        //else if block is marked, calculate new address
        else if (_blocks[i].mark_phase == _phase) {
            *((size_t *)_blocks[i].object - 1) = new_curr_block;
            _blocks[new_curr_block] = _blocks[i];
            void *mem = _memory + new_alloc_size;
            _blocks[new_curr_block].new_object = (Object *)((size_t *)mem + 1);
            new_alloc_size += _blocks[i].size + sizeof(size_t);
            _free_index += _blocks[i].size + sizeof(size_t);
            ++new_curr_block;
        }

        //else delete object
        else if (!_blocks[i].deleted) {
            delete _blocks[i].object;
        }
    }

    //adjust pointers
    root_p = _roots[_root_free].prev;
    while (root_p) {
        _basic_ptr *ptr = _roots[root_p].ptr;
        _adjust(ptr);
        root_p = _roots[root_p].prev;
    }

    //move marked objects
    for(i = 0; i < new_curr_block; ++i) {
        if (!_blocks[i].locked) {
            *((size_t *)_blocks[i].new_object - 1) = i;
            memcpy(_blocks[i].new_object, _blocks[i].object, _blocks[i].size);
            _blocks[i].object = _blocks[i].new_object;
        }
    }

    //result is number of freed bytes
    size_t freed_bytes = _alloc_size - new_alloc_size;

    //store new statistics for next GC phase
    _alloc_size = new_alloc_size;
    _curr_block = new_curr_block;

    return freed_bytes;
}


//allocate memory
static void *_alloc(size_t size)
{
    //if there are no more blocks free, collect
    if (_curr_block == _MAX_BLOCKS && !_collect()) return 0;

    //fix size to include header information and be aligned to 8 bytes
    size = ((size + sizeof(size_t) + 7) >> 3) << 3;

    //if there is not enough memory, collect
    if (_free_index + size > GC_MEMORY_SIZE && !_collect()) return 0;

    //calculate address of allocated memory
    void *mem = _memory + _free_index;

    //allocate memory
    _free_index += size;
    _alloc_size += size;

    //register memory block
    _block *block = &_blocks[_curr_block];
    block->object = (Object *)((size_t *)mem + 1);
    block->ptrs = 0;
    block->size = size - sizeof(size_t);
    block->mark_phase = _phase;
    block->adjust_phase = _phase;
    block->locked = 1;
    block->deleted = 0;

    //link memory block to block entry
    *(size_t *)mem = _curr_block;
    ++_curr_block;

    return (size_t *)mem + 1;
}


//free memory block
static void _free(void *p)
{
    _blocks[*((size_t *)p - 1)].deleted = 1;
}


//unlocks an object
static void _unlock(void *p)
{
    _blocks[*((size_t *)p - 1)].locked = 0;
}


//add root pointer
static void _add_root_ptr(_basic_ptr *ptr)
{
    //allocate a node from the already-existing double-linked list of nodes
    ptr->index = _root_free;
    _roots[_root_free].ptr = ptr;
    _root_free = _roots[_root_free].next;

    //if there are no more nodes, link the list of deleted nodes
    //to the front of the allocated nodes list
    if (!_root_free) {
        //if there are no more roots, exit
        if (!_root_deleted) {
            fprintf(stderr, "gc: out of root set memory\n");
            exit(-1);
        }

        //link the deleted node right after the node for given pointer
        _roots[_root_deleted].prev = ptr->index;
        _roots[ptr->index].next = _root_deleted;
        _root_free = _root_deleted;

        //no more deleted nodes
        _root_deleted = 0;
    }
}


//delete root pointer
static void _del_root_ptr(_basic_ptr *ptr)
{
    //remove from list of roots
    _roots[_roots[ptr->index].prev].next = _roots[ptr->index].next;
    _roots[_roots[ptr->index].next].prev = _roots[ptr->index].prev;

    //insert root node in list of deleted
    _roots[_root_deleted].prev = ptr->index;
    _roots[ptr->index].next = _root_deleted;
    _root_deleted = ptr->index;
}


//add pointer
static void _add_ptr(_basic_ptr *ptr)
{
    //if inside the gc memory, then find block that it belongs
    if (ptr >= (void *)_memory && ptr < (void *)(_memory + GC_MEMORY_SIZE)) {

        //search locked blocks in the reverse order they are created
        for(int i = _curr_block - 1; i >= 0; --i) {

            //if the pointer is inside the block, then add it in the list of
            //pointers the block has
            if (ptr >= (void *)_blocks[i].object &&
                ptr <  (void *)((char *)_blocks[i].object + _blocks[i].size)) {
                ptr->index = _blocks[i].ptrs;
                ptr->root = 0;
                _blocks[i].ptrs = (char *)ptr - (char *)_blocks[i].object;
                return;
            }
        }

        //internal error!
        fprintf(stderr, "gc: internal error: invalid member pointer\n");
        exit(-1);
    }

    //otherwise it is a root pointer
    else {
        _add_root_ptr(ptr);
        ptr->root = 1;
    }
}


//dynamic initialization
__library::__library()
{
    //initialize locking
    initLock();

    //put root pointer entries in a double-linked list
    _roots[0].prev = 0;
    _roots[0].next = 1;
    for(size_t i = 1; i < _MAX_ROOTS - 1; ++i) {
        _roots[i].prev = i - 1;
        _roots[i].next = i + 1;
    }
    _roots[_MAX_ROOTS - 1].prev = _MAX_ROOTS - 2;
    _roots[_MAX_ROOTS - 1].next = 0;
}


//clean up
__library::~__library()
{
    //finalize all blocks
    lock();
    for(int i = _curr_block - 1; i >= 0; --i) {
        if (!_blocks[i].deleted) delete _blocks[i].object;
    }
    unlock();

    //no more lock
    deleteLock();
}


//constructor
_library::_library()
{
    static __library library;
}


//default constructor
_ptr::_ptr(Object *obj)
{
    object = obj;
    lock();
    if (obj) _unlock(obj);
    _add_ptr(this);
    unlock();
}


//copy constructor
_ptr::_ptr(const _ptr &ptr)
{
    lock();
    _add_ptr(this);
    unlock();
}


//destructor
_ptr::~_ptr()
{
    lock();
    if (root) _del_root_ptr(this);
    unlock();
}


//assignment from raw pointer
void _ptr::operator = (Object *obj)
{
    if (obj == object) return;
    object = obj;
    if (!obj) return;
    lock();
    _unlock(obj);
    unlock();
}


/*****************************************************************************
    PUBLIC
 *****************************************************************************/


///allocate object
void *Object::operator new(size_t size)
{
    lock();
    void *mem = _alloc(size);
    unlock();
    return mem;
}


///delete object
void Object::operator delete(void *p)
{
    lock();
    _free(p);
    unlock();
}


/** Does garbage collection.
    @return number of bytes that were freed.
 */
size_t collectGarbage()
{
    lock();
    size_t freed_bytes = _collect();
    unlock();
    return freed_bytes;
}


} //end of namespace
