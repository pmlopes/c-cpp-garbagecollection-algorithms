#ifndef GCPTR_H
#define GCPTR_H 1

/**
 * Base class for all classes that support reference counting
 */
class GCObject {
public:
  GCObject() : mCount(0) {}
  virtual ~GCObject() {}

  inline void grab() const { ++mCount; }
  inline void release() const { if (--mCount == 0) delete (GCObject *) this; }

private:
  mutable unsigned int mCount;
};

/**
 * A reference counting-managed pointer for classes derived from GCObject which can
 * be used as C pointer
 */
template<class T> class GCPtr {
public:
  //Construct using a C pointer
  //e.g. GCPtr<T> x = new T();
  GCPtr(T* ptr = NULL) : mPtr(ptr) {
    if (ptr != NULL) {
      ptr->grab();
    }
  }

  //Copy constructor
  GCPtr(const GCPtr &ptr) : mPtr(ptr.mPtr) {
    if (mPtr != NULL) {
      mPtr->grab();
    }
  }

  ~GCPtr() {
    if (mPtr != NULL) {
      mPtr->release();
    }
  }

  //Assign a pointer
  //e.g. x = new T();
  GCPtr & operator=(T* ptr) {
    if (ptr != NULL) {
      ptr->grab();
    }
    if (mPtr != NULL) {
      mPtr->release();
    }
    mPtr = ptr;
    return (*this);
  }

  //Assign another GCPtr
  GCPtr & operator=(const GCPtr &ptr) {
    return (*this) = ptr.mPtr;
  }

  //Retrieve actual pointer
  T* get() const {
    return mPtr;
  }

  //Some overloaded operators to facilitate dealing with an GCPtr as a convetional C pointer.
  //Without these operators, one can still use the less transparent get() method to access the pointer.
  T* operator->() const {
    return mPtr;
  } //x->member
  T & operator*() const {
    return *mPtr;
  } //*x, (*x).member
  operator T*() const {
    return mPtr;
  } //T* y = x;
  operator bool() const {
    return mPtr != NULL;
  } //if(x) {/*x is not NULL*/}
  bool operator==(const GCPtr &ptr) {
    return mPtr == ptr.mPtr;
  }
  bool operator==(const T *ptr) {
    return mPtr == ptr;
  }

  private:
    T *mPtr; //Actual pointer
};
#endif
