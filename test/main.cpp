//
// Created by dguco on 19-3-23.
//

#include <iostream>
#include <functional>
#include <utility>      // std::declval
#include <iostream>     // std::cout

template<typename Res, typename ... Args>
struct Func
{
    std::function<Res(Args...)> func;
    Func(std::function<Res(Args...)> &&func)
    {
        func = std::move(func);
    }
};

struct A
{              // abstract class
    virtual int value() = 0;
};

struct C
{
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
    C *ccc = new C;
    type_c __c = std::move(*ccc);
    std::cout << a << '\n';
    return 0;
}