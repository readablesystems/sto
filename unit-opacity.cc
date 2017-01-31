#include <iostream>
#include <thread>

#include "TArray.hh"
#include "TBox.hh"

constexpr int array_size = 512;

typedef TArray<int, array_size> array_type;

void writer(array_type& arr) {
    std::cout << "writer advances with steps of 2." << std::endl;
    for (int n = 0; n < 10000; ++n) {
        TRANSACTION {
            int k = 0;
            int idx = 2*k+1;
            while (idx < array_size) {
                arr[idx] = arr[idx] + 1;
                ++k;
                idx = 2*k+1;
            }
        } RETRY(true);
    }
    std::cout << "writer finished." << std::endl;
    return;
}

void reader(array_type& arr) {
    std::cout << "reader advances with steps of 4." << std::endl;
    for (int n = 0; n < 10000; ++n) {
        TRANSACTION {
            int k = 0;
            int idx = 4*k+1;
            int val = -1;
            while (idx < array_size) {
                int v_ = arr[idx];
                if (val == -1)
                    val = v_;
                else if (val != v_) {
                    std::cerr << "Error at index " << idx
                        << ", expecting " << val << ", got "
                        << v_ << "." << std::endl;
                    abort();
                }

                ++k;
                idx = 4*k+1;
            }
        } RETRY(true);
    }
    std::cout << "reader finished." << std::endl;
}

void array_init(array_type& arr) {
    for (int i = 0; i < array_size; ++i)
        arr.nontrans_put(i, 0);
}

int main() {
    array_type arr;
    array_init(arr);

    auto tw = std::thread(writer, std::ref(arr));
    auto tr = std::thread(reader, std::ref(arr));

    tw.join();
    tr.join();

    std::cout << "Test pass." << std::endl;

    return 0;
}
