stagefuture
=======
- stagefuture 基于c++11的可穿行的future类，基于下面的项目修改的（修改中）。<br>
  [项目地址](https://github.com/Amanieu/asyncplusplus)
- 实例:初版  
```
using namespace stagefuture;
int main(int argc, char *argv[])
{
    //testSort();
    int test_a = 10;
    threadpool_scheduler scheduler(1);
    single_thread_scheduler singleThreadScheduler;
    int a = 0;
    stage_future<void> task1 = stagefuture::run_async(singleThreadScheduler,
                                                      [test_a]() -> void
                                                      {
                                                          std::cout
                                                              << "Create Task 1 executes asynchronously,test_a : "
                                                              << test_a
                                                              << std::endl;
                                                      });

    std::string str = "100";
    stage_future<int> task11 =
        stagefuture::supply_async(scheduler,
                                  [&singleThreadScheduler, &str]() -> stage_future<int>
                                  {
                                      std::string str1 = std::to_string(std::stoi(str) * 100);
                                      std::cout
                                          << "=======create task11========="
                                          << str1
                                          << std::endl;
                                      stage_future<int>
                                          res = stagefuture::supply_async(singleThreadScheduler, [str1]() -> int
                                      {
                                          std::cout
                                              << "======== in create task11 "
                                              << str1.data()
                                              << "========"
                                              << std::endl;
                                          return std::stoi(str1);
                                      });
                                      std::cout
                                          << "=======create task11 end ========="
                                          << std::endl;
                                      return res;
                                  });

    stage_future<std::string> ttt =
        task11.thenApply([&scheduler](int value) -> stage_future<std::string>
                         {
                             value *= 100;
                             auto res = stagefuture::supply_async(scheduler, [value]() -> std::string
                             {
                                 std::cout
                                     << "=======create ttt========="
                                     << "value: "
                                     << value
                                     << std::endl;
                                 return std::to_string(value);
                             });
                             value *= 100;
                             return res;
                         });

    std::cout
        << "****************************************************" << std::endl;
    ttt.thenAccept([](std::string value) -> void
                   {
                       std::cout
                           << "Task ttt executes in parallel with stage_future 1"
                           << value
                           << std::endl;
                   });

    stage_future<int> task2 = stagefuture::supply_async(singleThreadScheduler,
                                                        []() -> int
                                                        {
                                                            std::cout
                                                                << "Task 2 executes in parallel with stage_future 1"
                                                                << " thread id " << std::this_thread::get_id()
                                                                << std::endl;
                                                            return 42;
                                                        });

    stage_future<int> task3 = task2.thenApply([](int value) -> int
                                              {
                                                  std::cout
                                                      << "Task 3 executes after stage_future 2, which returned "
                                                      << value
                                                      << " thread id " << std::this_thread::get_id()
                                                      << std::endl;
                                                  return value * 3;
                                              });
    stage_future<std::tuple<stagefuture::stage_future<void>,
                            stagefuture::stage_future<int>>> task4 = stagefuture::when_all(task1, task3);
    stage_future<void> task5 = task4.thenAccept([](std::tuple<stagefuture::stage_future<void>,
                                                              stagefuture::stage_future<int>> results)
                                                {
                                                    std::cout
                                                        << "Task 5 executes after tasks 1 and 3. Task 3 returned "
                                                        << std::get<1>(results).get()
                                                        << " thread id " << std::this_thread::get_id()
                                                        << std::endl;
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
