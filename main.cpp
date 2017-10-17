//
//  main.cpp
//  mythread
//
//  Created by 杜国超 on 17/9/7.
//
//

#include <iostream>
#include <vector>
#include <chrono>
#include "threadpool.h"

int main()
{
    
    CThreadPool pool;
    std::vector< std::future<int> > results;
    
    for(int i = 0; i < 10000; ++i) {
        results.emplace_back(pool.PushTaskBack([i] {
            std::cout << "i = " << i  << " thread id = " << this_thread::get_id() << std::endl;
            return i*i;
        }));
    }

    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
    
    return 0;
}
