#include <iostream>
#include "gcPtr.h"

using namespace std;

class MyClass : public GCObject
{
public:
        ~MyClass() {cout << this << " is no longer needed" << endl;}
        void print() {cout << "Hello" << endl;}
};

GCPtr<MyClass> g;

void print(MyClass* myClass) {
  myClass->print();
  g = myClass;
}

void gprint() {
  g->print();
  g = NULL;
}
int main(void)
{
#if 0
  //1: Demonstrate RCBase class

  //Module 1 creates an object
  MyClass *a = new MyClass();
  a->grab();      //RC=1

  //Module 2 grabs object
  MyClass* ptr = a;
  ptr->grab();    //RC=2

  //Module 1 no longer needs object
  a->release();      //RC=1
  a = NULL;

  //Module 2 no longer needs object
  ptr->release();    //object will be destroyed here
  ptr = NULL;
#endif
  //==================================================
  //2: Demonstrate RCPtr

  //Module 1 creates an object
  GCPtr<MyClass> a2 = new MyClass();      //RC=1

  //Module 2 grabs object
  GCPtr<MyClass> ptr2 = a2;   //RC=2

  //Module 2 invokes a method
  ptr2->print();
  (*ptr2).print();
  print(ptr2);

  //Module 1 no longer needs object
  a2 = NULL;      //RC=1

  //Module 2 no longer needs object
  ptr2 = NULL;    //object will be destroyed here
  gprint();
}
