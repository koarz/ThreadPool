#include "thread_pool.hpp"

#include <chrono>
#include <format>
#include <iostream>
#include <list>
#include <random>

using namespace std;

// 为什么开这么多线程
// 最开始测试时以为是线程池实现有问题，经过测试以及思考之后意识到问题不在线程池这
// 排序代码对线程池不友好，在递归中会阻塞当前线程，所以当线程池中的线程不够递归时，整个线程池所有线程被阻塞，线程池会停止工作
// 最后测试下来，使用了线程池的排序在多线程情况下性能更优但是不如单线程排序，应该是多线程不适合做排序工作
koarz::ThreadPool thread_pool(800);

template <typename T> std::list<T> pool_thread_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  result.splice(result.begin(), input, input.begin());
  T const &partition_val = *result.begin();
  typename std::list<T>::iterator divide_point =
      std::partition(input.begin(), input.end(),
                     [&](T const &val) { return val < partition_val; });
  std::list<T> new_lower_chunk;
  new_lower_chunk.splice(new_lower_chunk.end(), input, input.begin(),
                         divide_point);
  std::future<std::list<T>> new_lower =
      thread_pool.commit(pool_thread_quick_sort<T>, new_lower_chunk);
  std::list<T> new_higher(pool_thread_quick_sort(input));
  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower.get());
  return result;
}

template <typename T>
std::list<T> single_thread_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  result.splice(result.begin(), input, input.begin());
  T const &partition_val = *result.begin();
  typename std::list<T>::iterator divide_point =
      std::partition(input.begin(), input.end(),
                     [&](T const &val) { return val < partition_val; });
  std::list<T> new_lower_chunk;
  new_lower_chunk.splice(new_lower_chunk.end(), input, input.begin(),
                         divide_point);
  std::list<T> new_lower = single_thread_quick_sort(new_lower_chunk);
  std::list<T> new_higher(single_thread_quick_sort(input));
  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower);
  return result;
}

template <typename T> std::list<T> parallel_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  result.splice(result.begin(), input, input.begin());
  T const &pivot = *result.begin();
  auto divide_point = std::partition(input.begin(), input.end(),
                                     [&](T const &t) { return t < pivot; });
  std::list<T> lower_part;
  lower_part.splice(lower_part.end(), input, input.begin(), divide_point);
  // ①因为lower_part是副本，所以并行操作不会引发逻辑错误，这里可以启动future做排序
  std::future<std::list<T>> new_lower(
      std::async(&parallel_quick_sort<T>, std::move(lower_part)));

  // ②
  auto new_higher(parallel_quick_sort(std::move(input)));
  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower.get());
  return result;
}

std::list<int> GetRadomList(unsigned int n) {
  std::list<int> ret;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0x80000000, 0x7fffffff);
  for (unsigned int i = 0; i < n; i++) {
    ret.emplace_back(distrib(gen));
  }
  return ret;
}

int main(int argc, char **argv) {
  std::list<int> nlist = GetRadomList(1000);
  std::cout << "Start Sort\n";
  {
    auto start_time = std::chrono::steady_clock::now();
    auto sortlist = pool_thread_quick_sort<int>(nlist);
    auto end_time = std::chrono::steady_clock::now();
    std::cout << std::format(
        "\nUsing ThreadPool Sort Spend {} Sorted {} nums\n",
        std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                              start_time),
        nlist.size());
  }
  {
    auto start_time = std::chrono::steady_clock::now();
    auto sortlist = single_thread_quick_sort<int>(nlist);
    auto end_time = std::chrono::steady_clock::now();
    std::cout << std::format(
        "\nSingleThread SortFunction Spend {} Sorted {} nums\n",
        std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                              start_time),
        nlist.size());
  }
  {

    auto start_time = std::chrono::steady_clock::now();
    auto sortlist = parallel_quick_sort<int>(nlist);
    auto end_time = std::chrono::steady_clock::now();
    std::cout << std::format(
        "\nMultiThread SortFunction Spend {} Sorted {} nums\n",
        std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                              start_time),
        nlist.size());
  }
  {
    auto start_time = std::chrono::steady_clock::now();
    nlist.sort();
    auto end_time = std::chrono::steady_clock::now();
    std::cout << std::format(
        "\nSTL SortFunction Spend {} Sorted {} nums\n",
        std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                              start_time),
        nlist.size());
  }
  return 0;
}
