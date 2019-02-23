stagefuture
=======
- stagefuture 基于c++11的可穿行的future类，基于下面的项目修改的。<br>
  [项目地址](https://github.com/Amanieu/asyncplusplus)
- 实例:初版  
```
int main(int argc, char *argv[])   
{  
    //testSort();  
    C *c = new C;  
    c->test();  
    int test_a = 10;  
    auto task1 = stagefuture::run_async([test_a]
                                        {
                                            std::cout << "Task 1 executes asynchronously,test_a * test_a: "
                                                      << test_a * test_a << std::endl;
                                        });
    auto task2 = stagefuture::supply_async([]() -> int
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
```
