# new_process_executor
An executor which creates execution by spawning new processes

# Demo

`new_process_executor` can create one-way and two-way execution in a separate, newly-created process:

```c++
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

int baz()
{
  std::cout << "baz() called in process " << this_process::get_id() << std::endl;

  std::string what = "Exception in baz() in process " + std::to_string(this_process::get_id());
  throw std::runtime_error(what);
}

int main()
{
  std::cout << "main() called in process " << this_process::get_id() << std::endl;

  new_process_executor exec;

  // call foo() in a newly-created process
  exec.execute(foo);

  // call bar() in a newly-created process
  interprocess_future<int> future = exec.twoway_execute(bar);
  int result = future.get();
  std::cout << "Received result " << result << " from another process." << std::endl;
  assert(result == 13);

  // call baz() in a newly-created process
  interprocess_future<int> future2 = exec.twoway_execute(baz);

  try
  {
    int result = future2.get();
    assert(0);
  }
  catch(interprocess_exception e)
  {
    std::cout << "Receivied exception [" << e.what() << "] from another process." << std::endl;
  }

  std::cout << "OK" << std::endl;
}
```

Program output:

```
$ clang -std=c++11 demo.cpp -lstdc++
$ ./a.out 
main() called in process 18784
foo() called in process 18785
bar() called in process 18786
Received result 13 from another process.
OK
```

