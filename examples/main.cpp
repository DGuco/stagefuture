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

using namespace stagefuture;

class A
{
public:
    virtual void test() = 0;
};

class B: public A
{

};

class C: public B
{
public:
    virtual ~C()
    {
        printf("~C\n");
    }
    void test() override
    {
        printf("Class c test func\n");
        delete this;
    }
};

#define SWAP(a, b) {int temp = a; a = b; b = temp;}
void quick_sort(int *a, int low, int high)
{
    if (low >= high) {
        return;
    }

    int left = low;
    int right = high;
    int flag = a[left];
    while (left < right) {
        while (left < right && a[right] >= flag) {
            right--;
        }
        a[left] = a[right];
        while (left < right && a[left] <= flag) {
            left++;
        }
        a[right] = a[left];
    }
    a[left] = flag;
    quick_sort(a, low, left - 1);
    quick_sort(a, left + 1, high);
}

void max_heapify(int arr[], int start, int end)
{
    //建立父节点指标和子节点指标
    int dad = start;
    int son = dad * 2 + 1;
    while (son <= end) {
        //若子节点指标在范围内才做比较
        if (son + 1 <= end && arr[son] < arr[son + 1]) //先比较两个子节点大小，选择最大的
            son++;
        if (arr[dad] > arr[son]) //如果父节点大於子节点代表调整完毕，直接跳出函数
            return;
        else { //否则交换父子内容再继续子节点和孙节点比较
            SWAP(arr[dad], arr[son]);
            dad = son;
            son = dad * 2 + 1;
        }
    }
}

void heap_sort(int arr[], int len)
{
    //初始化，i从最後一个父节点开始调整
    for (int i = len / 2 - 1; i >= 0; i--)
        max_heapify(arr, i, len - 1);
    //先将第一个元素和已经排好的元素前一位做交换，再从新调整(刚调整的元素之前的元素)，直到排序完毕
    for (int i = len - 1; i > 0; i--) {
        SWAP(arr[0], arr[i]);
        max_heapify(arr, 0, i - 1);
    }
}

int main(int argc, char *argv[])
{
    int a[] = {3, 5, 3, 0, 8, 6, 1, 5, 8, 6, 2, 4, 9, 4, 7, 0, 1, 8, 9, 7, 3, 1, 2, 5, 9, 7, 4, 0, 2, 6};
    quick_sort(a, 0, sizeof(a) / sizeof(a[0]) - 1);/*这里原文第三个参数要减1否则内存越界*/

    for (int i = 0; i < sizeof(a) / sizeof(a[0]); i++) {
        std::cout << a[i] << " ";
    }
    std::cout << std::endl;
    int arr[] = {3, 5, 3, 0, 8, 6, 1, 5, 8, 6, 2, 4, 9, 4, 7, 0, 1, 8, 9, 7, 3, 1, 2, 5, 9, 7, 4, 0, 2, 6};
    int len = (int) sizeof(arr) / sizeof(*arr);
    heap_sort(arr, len);
    for (int i = 0; i < len; i++)
        std::cout << arr[i] << ' ';
    printf("\n");
    return 0;
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
