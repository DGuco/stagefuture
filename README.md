# stagefuture
- stagefuture 基于c++11的可穿行的future类（修改中），参考以下项目。<br>
  [项目地址](https://github.com/Amanieu/asyncplusplus)，修改其部分实现，参考java CompletableFuture类重写其api
# 测试示例:初版  
- 测试环境 ubuntu 12.04 g++ 版本 6.2.0 clion 2017
- [测试示例源码](https://github.com/DGuco/stagefuture/blob/master/examples/main.cpp)
- 测试示例
```
#include "stagefuture.h"
#include <iostream>
#include <chrono>
#include <string>
#include <zconf.h>

using namespace stagefuture;

//线程池(为了输出的有序性和验证程序执行线程的正确性这里线程池线程数设为1)
static threadpool_scheduler *g_scheduler = new threadpool_scheduler(1);

//单线程
static single_thread_scheduler *g_singleScheduler = new single_thread_scheduler;

void testRunAsync()
{
    printf("===============================testRunAsync====================================\n");
    stage_future<void> void_future =
        stagefuture::run_async([]()
                               {
                                   printf("Create void future asynchronously,thread id %lld\n",
                                          std::this_thread::get_id());

                               }, *g_scheduler);
    // void_future.get(); will block in there 会阻塞在这里
    usleep(100000);  //wait run over
}

void testSupplyAsync()
{
    printf("===============================testSupplyAsync====================================\n");
    std::string str = "1000";
    stage_future<int> no_void_future =
        stagefuture::supply_async<int>([str]()
                                       {
                                           printf("Create no void future asynchronously,thread id %lld\n",
                                                  std::this_thread::get_id());
                                           return std::stoi(str);
                                       }, *g_scheduler);
    // no_void_future.get(); will block in there 会阻塞在这里
    usleep(100000);  //wait run over
}

void testThenAccept()
{
    printf("===============================testThenAccept====================================\n");
    stage_future<int> task1 =
        stagefuture::supply_async<int>([]()
                                       {
                                           printf("Create task1 asynchronously,thread id %lld\n",
                                                  std::this_thread::get_id());
                                           return 100;
                                       }, *g_scheduler);
    stage_future<void> task2 =
        task1.thenAccept([](int value)
                         {
                             printf("Consume task1 asynchronously,thread id %lld\n",
                                    std::this_thread::get_id());
                             printf("task 1 res %d\n", value);
                         });

    stage_future<void> task3 =
        task2.thenAcceptAsync([]()
                              {
                                  printf("Consume task2 asynchronously,thread id %lld\n",
                                         std::this_thread::get_id());
                              }, *g_singleScheduler);
    usleep(100000);  //wait run over
}

void testThenApply()
{
    printf("===============================testThenApply====================================\n");
    stage_future<int> task1 =
        stagefuture::supply_async<int>([]()
                                       {
                                           printf("Create task1 asynchronously,thread id %lld\n",
                                                  std::this_thread::get_id());
                                           return 100;
                                       }, *g_scheduler);
    stage_future<std::string> task2 =
        task1.thenApply<std::string>([](int value)
                                     {
                                         printf("Apply task1 asynchronously,thread id %lld\n",
                                                std::this_thread::get_id());
                                         printf("task 1 res %d\n", value);
                                         return std::to_string(value * value);
                                     });
    stage_future<void> task3 =
        task2.thenAcceptAsync([](std::string value)
                              {
                                  printf("Apply task2 asynchronously,thread id %lld\n", std::this_thread::get_id());
                                  printf("task 1 res %s\n", value.data());
                              }, *g_singleScheduler);

    task3.thenApplyAsync<int>([]()
                              {
                                  printf("Apply task3 asynchronously,thread id %lld\n", std::this_thread::get_id());
                                  return 0;
                              });
    usleep(100000);  //wait run over
}

void testThenCompose()
{
    printf("===============================testThenCompose====================================\n");
    stage_future<int> task1 =
        stagefuture::supply_async<int>([]()
                                       {
                                           printf("Create task1 asynchronously,thread id %lld\n",
                                                  std::this_thread::get_id());
                                           return 100;
                                       }, *g_scheduler);
    stage_future<std::string> task2 = task1.thenComposeAsync<std::string>
        ([](int value) -> stage_future<std::string>
         {
             printf("Compose task1 asynchronously,thread id %lld\n", std::this_thread::get_id());
             //.. DO SOME THING
             auto future =
                 stagefuture::supply_async<std::string>([value]()
                                                        {
                                                            int res_value = value * value;
                                                            return std::to_string(res_value);
                                                        }, *g_singleScheduler);
             return future;
         });
    usleep(100000);  //wait run over
}

void testThenCombine()
{
    printf("===============================testThenCombine====================================\n");
    stage_future<float> task1 =
        stagefuture::supply_async<float>([]()
                                         {
                                             printf("Create task1 asynchronously,thread id %lld\n",
                                                    std::this_thread::get_id());
                                             return 100.f;
                                         }, *g_scheduler);
    stage_future<std::string> task2 =
        stagefuture::supply_async<std::string>([]()
                                               {
                                                   printf("Create task2 asynchronously,thread id %lld\n",
                                                          std::this_thread::get_id());
                                                   return "100";
                                               });
    stage_future<long> task3 =
        task2.thenCombineAsync<long, float>(std::move(task1), [](std::string res1, float res2)
        {
            printf("Combine  task1 and task 2 asynchronously,thread id %lld\n", std::this_thread::get_id());
            return std::stoi(res1) * res2 - 200;
        });
    task3.thenAccept([](long value)
                     {
                         printf("Consume task3 asynchronously,thread id %lld\n",
                                std::this_thread::get_id());
                         printf("task 3 res %ld\n", value);
                     });
    usleep(100000);  //wait run over
}

void testOther()
{
    printf("===============================testOter====================================\n");
    stage_future<void> task1 =
        stagefuture::run_async([]()
                               {
                                   printf("Create task1 asynchronously,thread id %lld\n",
                                          std::this_thread::get_id());
                               });

    stage_future<int> task3 =
        stagefuture::supply_async<int>([]()
                                       {
                                           printf("Create task3 asynchronously,thread id %lld\n",
                                                  std::this_thread::get_id());
                                           return 100;
                                       }, *g_scheduler);

    stage_future<std::tuple<stagefuture::stage_future<void>,
                            stagefuture::stage_future<int>>> task4 = stagefuture::when_all(task1, task3);
    stage_future<void> task5
        = task4.thenAccept([](std::tuple<stagefuture::stage_future<void>,
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
int main(int argc, char *argv[])
{
    testRunAsync();
    testSupplyAsync();
    testThenAccept();
    testThenApply();
    testThenCompose();
    testThenCombine();
    testOther();
}

```
- 测试结果
````
===============================testRunAsync====================================
Create void future asynchronously,thread id 140653441095424
===============================testSupplyAsync====================================
Create no void future asynchronously,thread id 140653441095424
===============================testThenAccept====================================
Create task1 asynchronously,thread id 140653441095424
Consume task1 asynchronously,thread id 140653441095424
task 1 res 100
Consume task2 asynchronously,thread id 140653432702720
===============================testThenApply====================================
Create task1 asynchronously,thread id 140653441095424
Apply task1 asynchronously,thread id 140653441095424
task 1 res 100
Apply task2 asynchronously,thread id 140653432702720
task 1 res 10000
Apply task3 asynchronously,thread id 140653415917312
===============================testThenCompose====================================
Create task1 asynchronously,thread id 140653441095424
Compose task1 asynchronously,thread id 140653424310016
===============================testThenCombine====================================
Create task1 asynchronously,thread id 140653441095424
Create task2 asynchronously,thread id 140653424310016
Combine  task1 and task 2 asynchronously,thread id 140653424310016
Consume task3 asynchronously,thread id 140653424310016
task 3 res 9800
===============================testOter====================================
Create task3 asynchronously,thread id 140653441095424
Create task1 asynchronously,thread id 140653191227136
Task 5 executes after tasks 1 and 3. Task 3 returned 100 thread id 140653191227136
Task 5 has completed
This is executed in parallel...
with this
01234
The sum of {1, 2, 3, 4} is 10
````

