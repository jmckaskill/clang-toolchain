#include <thread>
#include <iostream>
#include <exception>

class my_exception : public std::exception {
  virtual const char* what() const noexcept {return "my_exception";}
};

int main() {
  std::cout << "hello from thread " << std::this_thread::get_id() << std::endl;

  std::thread t([]{std::cout << "hello from thread " << std::this_thread::get_id() << std::endl;});
  t.join();

  try {
    throw my_exception();
  } catch (std::exception& e) {
    std::cout << "caught " << e.what() << std::endl;
  }
  return 0;
}


