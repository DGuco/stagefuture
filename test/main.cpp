//
// Created by dguco on 19-3-23.
//

#include <iostream>
#include <functional>

template<typename Res, typename ... Args>
struct Func
{
    std::function<Res(Args...)> func;
    Func(std::function<Res(Args...)> &&func)
    {
        func = std::move(func);
    }
};

int main()
{
    printf("Hello word\n");
}