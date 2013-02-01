#if 0
#include "gc.h"
using namespace gc;

#include <iostream>
using namespace std;


#include <time.h>
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/*****************************************************************************
    TEST CLASSES
 *****************************************************************************/


class Bar;
class Foo;
class Bar1;
class Foo1;


//big
class Big : public Object {
public:
    int m_data[500];
};


//used for reference testing
class Bar : public Object {
public:
    Pointer<Foo> foo;

    ~Bar() {
        cout << "~Bar\n";
    }
};


//used for reference testing
class Foo : public Object {
public:
    Pointer<Bar> bar;

    ~Foo() {
        cout << "~Foo\n";
    }
};


//simple class
class Simple : public Object {
public:
};


//used for reference testing
class Bar1 : public Object {
public:
    Pointer<Foo1> foo1;
};


//used for reference testing
class Foo1 : public Object {
public:
    Pointer<Bar1> bar1;
};


//complex class
class Complex : public Object {
public:
    Pointer<Big> ptr1;
    Pointer<Bar1> bar1;
    Pointer<Foo1> foo1;

    Complex() : ptr1(new Big), bar1(new Bar1), foo1(new Foo1) {
        bar1->foo1 = foo1;
        foo1->bar1 = bar1;
    }
};


/*****************************************************************************
    SINGLETHREADED TEST
 *****************************************************************************/


//test performance
void test_performance()
{
    //pointer to avoid dangling refs
    Pointer<Big> p1;

    clock_t start = clock();

    //do test and get the average
    int c;
    for(c = 0; c < 100; c++) {
        for(int i = 0; i < 65000; i++) {
            p1 = new Big;
        }
    }

    clock_t end = clock();

    cout << ((end - start) / c) << endl;
}


//test performance
void test_performance_complex()
{
    //pointer to avoid dangling refs
    Pointer<Complex> p1;

    clock_t start = clock();

    //do test and get the average
    int c;
    for(c = 0; c < 100; c++) {
        for(int i = 0; i < 65000; i++) {
            p1 = new Complex;
        }
    }

    clock_t end = clock();

    cout << ((end - start) / c) << endl;
}


//for testing root initialization
Pointer<Bar> root_bar;


//test root reference
void test_root_reference()
{
    root_bar = new Bar;
    collectGarbage();
    root_bar->foo = new Foo;
}


//test cyclic reference
void test_cyclic_reference()
{
    {
        Pointer<Foo> foo = new Foo;
        Pointer<Bar> bar = new Bar;
        foo->bar = bar;
        bar->foo = foo;
    }
    collectGarbage();
}


//test stack based reference
void test_stack_reference()
{
    Foo foo;
    foo.bar = new Bar;
    collectGarbage();
}


/*****************************************************************************
    MULTITHREADED TEST
 *****************************************************************************/

#ifdef WIN32
//thread procedure
DWORD CALLBACK thread_proc(LPVOID params)
{
    //pointer so as that new objects are not dangling
    Pointer<Simple> p1;

    //repeat enough times
    for(int j = 0; j < 10000; j++) {
        for(int i = 0; i < 65000; i++) {
            p1 = new Simple;
        }
    }

    cout << "thread finished\n";
    return 0;
}

//multithreaded test
void test_multithreaded()
{
    CreateThread(0, 0, thread_proc, 0, 0, 0);
    thread_proc(0);
}
#else
//thread procedure
void *thread_proc(void *params)
{
    //pointer so as that new objects are not dangling
    Pointer<Simple> p1;

    //repeat enough times
    for(int j = 0; j < 10000; j++) {
        for(int i = 0; i < 65000; i++) {
            p1 = new Simple;
        }
    }

    cout << "thread finished\n";
    return 0;
}

//multithreaded test
static pthread_t gcThread;

void test_multithreaded()
{
    pthread_create(&gcThread, NULL, thread_proc, NULL);
}
#endif


/*****************************************************************************
    MAIN
 *****************************************************************************/


int main()
{
    //test_performance();
    test_performance_complex();
    //test_root_reference();
    //test_cyclic_reference();
    //test_stack_reference();
    //test_multithreaded();
    return 0;
}
#endif
