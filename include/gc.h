#ifndef GC_HPP
#define GC_HPP

#include <stddef.h>

namespace gc {


///defined for a multithreaded garbage collector
#ifndef GC_MULTITHREADED
#define GC_MULTITHREADED     0
#endif


///Memory size in bytes for the garbage collector
#ifndef GC_MEMORY_SIZE
#define GC_MEMORY_SIZE       (1024 * 1024 * 64)
#endif //GC_MEMORY_SIZE


class Object;


//library
struct _library {
    //constructor
    _library();
};


//basic ptr
struct _basic_ptr : _library {
    Object *object;
    size_t index:31;
    size_t root:1;
};


//pointer
struct _ptr : _basic_ptr {
    //default constructor
    _ptr(Object *obj = 0);

    //copy constructor
    _ptr(const _ptr &ptr);

    //destructor
    ~_ptr();

    //assignment from raw pointer
    void operator = (Object *obj);

    //assignment from pointer
    void operator = (const _ptr &ptr) {
        object = ptr.object;
    }
};


/** Base class for all garbage collected objects.
    It must be the first class in the inheritance tree.
 */
class Object : _library {
public:
    /** destructor is virtual in order to properly finalize objects.
     */
    virtual ~Object() {
    }

    /** allocates a garbage-collected object.
        @param size size of object in bytes.
        @return pointer to allocated memory or null if out of memory.
     */
    void *operator new(size_t size);

    /** deletes a garbage-collected object.
        @param p pointer to object to free.
     */
    void operator delete(void *p);

private:
    ///these operations are not allowed
    void *operator new[](size_t);
    void operator delete[](void *);
};


/** A garbage-collected pointer.
    @param T type of garbage-collected object; it must be derived from class
        Object.
 */
template <class T> class Pointer : _ptr {
public:
    /** The default constructor.
        @param p pointer to object.
     */
    Pointer(T *p = 0) : _ptr(p) {
    }

    /** The copy constructor.
        @param p source object.
     */
    Pointer(const Pointer<T> &p) : _ptr(p) {
    }

    /** Retrieves the pointer value.
        @return a raw pointer to object of type T; it may be null.
     */
    T *operator ()() const {
        return (T *)object;
    }

    /** Automatic conversion to raw pointer.
        @return a raw pointer to object of type T; it may be null.
     */
    operator T *() const {
        return (T *)object;
    }

    /** Access to the pointed object's members.
        @return a raw pointer to object of type T; it may be null.
     */
    T *operator ->() const {
        return (T *)object;
    }

    /** The equal-to comparison operator with pointer.
        @param p pointer to compare to this.
        @return true if this and given object point to the same object.
     */
    bool operator == (const T *p) const {
        return object == p;
    }

    /** The different-than comparison operator with pointer.
        @param p pointer to compare to this.
        @return true if this and given object point to different objects.
     */
    bool operator != (const T *p) const {
        return object != p;
    }

    /** The equal-to comparison operator with pointer.
        @param p pointer to compare to this.
        @return true if this and given object point to the same object.
     */
    bool operator == (const Pointer<T> &p) const {
        return object == p.object;
    }

    /** The different-than comparison operator with pointer.
        @param p pointer to compare to this.
        @return true if this and given object point to different objects.
     */
    bool operator != (const Pointer<T> &p) const {
        return object != p.object;
    }

    /** assignment from raw pointer.
        @param p raw pointer.
        @return reference to this.
     */
    Pointer<T> &operator = (T *p) {
        _ptr::operator = (p);
        return *this;
    }

    /** assignment from pointer object.
        @param p pointer.
        @return reference to this.
     */
    Pointer<T> &operator = (const Pointer<T> &p) {
        _ptr::operator = (p);
        return *this;
    }
};


/** Does garbage collection.
    @return number of bytes that were freed.
 */
size_t collectGarbage();


} //end of namespace


#endif //GC_HPP
