// Copyright (c) 2015-2019 Amanieu d'Antras and DGuco(杜国超).All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stagefuture.h"
#include <iostream>
#include <chrono>
#include <string>


int main(int argc, char *argv[])
{
    auto task1 = stagefuture::spawn([]
                                           {
                                               std::cout << "Task 1 executes asynchronously" << std::endl;
                                           });
    auto task2 = stagefuture::spawn([]() -> int
                                           {
                                               std::cout << "Task 2 executes in parallel with stage_future 1"
                                                         << std::endl;
                                               return 42;
                                           });
    auto task3 = task2.then([](int value) -> int
                            {
                                std::cout << "Task 3 executes after stage_future 2, which returned "
                                          << value << std::endl;
                                return value * 3;
                            });
    auto task4 = stagefuture::when_all(task1, task3);
    auto
        task5 = task4.then([](std::tuple<stagefuture::stage_future<void>,
                                         stagefuture::stage_future<int>>
                              results)
                           {
                               std::cout << "Task 5 executes after tasks 1 and 3. Task 3 returned "
                                         << std::get<1>(results).get() << std::endl;
                           });

    task5.get();
    std::cout << "Task 5 has completed" << std::endl;

    stagefuture::parallel_invoke([]
                                 {
                                     std::cout << "This is executed in parallel..." << std::endl;
                                 }, []
                                 {
                                     std::cout << "with this" << std::endl;
                                 });

    stagefuture::parallel_for(stagefuture::irange(0, 5), [](int x)
    {
        std::cout << x;
    });
    std::cout << std::endl;

    int r = stagefuture::parallel_reduce({1, 2, 3, 4}, 0, [](int x, int y)
    {
        return x + y;
    });
    std::cout << "The sum of {1, 2, 3, 4} is " << r << std::endl;
}
