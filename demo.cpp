#include <iostream>
#include <cassert>

#include "new_process_executor.hpp"

void foo()
{
  std::cout << "foo() called in process " << this_process::get_id() << std::endl;
}

int bar()
{
  std::cout << "bar() called in process " << this_process::get_id() << std::endl;
  return 13;
}

int main()
{
  std::cout << "main() called in process " << this_process::get_id() << std::endl;

  new_process_executor exec;

  // call foo() in a newly-created process
  exec.execute(foo);

  // call bar() in a newly-created process
  interprocess_future<int> future = exec.twoway_execute(bar);
  assert(future.get() == 13);

  std::cout << "OK" << std::endl;
}

