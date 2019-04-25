//
// Created by dguco on 19-3-23.
//

#include <iostream>
#include <functional>
#include <utility>      // std::declval
#include <iostream>     // std::cout

struct A
{              // abstract class
    virtual int value() = 0;
};

struct C
{
    C()
    {

    }
    ~C(){
        printf("~~~~~\n");
    }
    float operator()()
    {

    }
    int a;
};

class B: public A
{    // class with specific constructor
    int val_;
public:
    B(int i, int j)
        : val_(i * j)
    {}
    int value()
    { return val_; }
};


int main()
{
    decltype(std::declval<A>().value()) a;  // int a
    decltype(std::declval<B>().value()) b;  // int b
    decltype(B(0, 0).value()) c;   // same as above (known constructor)
    a = b = B(10, 2).value();
    typedef decltype(std::declval<C>()) type_c;
    typedef decltype(std::declval<C>()()) type_cc;
    C *ccc = new C;
    type_c __c = std::move(*ccc);
    type_cc __cc = 0.001f;
    std::cout << a << '\n';
    std::function<void()> func;
    {
        std::string sss("22222222");
        func = [&sss]() -> void
        {
            std::cout << "func" << sss.data() << std::endl;
        };
    }
    func();
    return 0;
}