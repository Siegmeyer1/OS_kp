#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/mman.h>
#include <cstdlib>
#include <thread>
#include <condition_variable>
#include <mutex>

std::mutex mutex;
std::condition_variable cv;

void reverse(unsigned int &start, unsigned int &end, unsigned char* mapping) {
    mutex.lock();
    int st = start, nd = end;
    cv.notify_all();
    mutex.unlock();
    int tmp;

    for (int i = st; i < nd; i++) {
            tmp = (int) mapping[i];
            mapping[i] = char(255 - tmp);
    }
}

void black_n_white(unsigned int &start, unsigned int &end, unsigned char* mapping) {
    mutex.lock();
    int st = start, nd = end;
    cv.notify_all();
    mutex.unlock();
    unsigned char tmp = 0;

    for (int i = st; i < nd; i+=3) {
        tmp = (unsigned char)((mapping[i] + mapping[i+1] + mapping[i+2])/3);

        mapping[i] = tmp;
        mapping[i+1] = tmp;
        mapping[i+2] = tmp;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "USAGE: ./main <file.pmp> <number of threads> <filter option>\n";
        return -1;
    }

    //=========================preparations==============================//

    std::ifstream file(argv[1], std::ios::binary);
    if (file.bad()) {
        std::cout << "Bad File\n";
        return -1;
    }
    u_short tmp2, depth;
    uint32_t tmp4, size, m_offset, width, height;
    file.read((char*)&tmp2, sizeof(tmp2));
    file.read((char*)&size, sizeof(size));
    file.read((char*)&tmp4, sizeof(tmp4));
    file.read((char*)&m_offset, sizeof(m_offset));
    file.read((char*)&tmp4, sizeof(tmp4));
    file.read((char*)&width, sizeof(width));
    file.read((char*)&height, sizeof(height));
    file.read((char*)&tmp2, sizeof(tmp2));
    file.read((char*)&depth, sizeof(depth));
    if (depth != 24) {
        std::cout << "unfortunately, this program doesn`t know how to process .bmp files \n"
                     "with each pixel coded NOT by 24 bits (byte per colour in RGB term)\n"
                     "please contact developer and say him that somehow your file has weird color coding/\n";
        return -1;
    }
    std::cout << "size of picture: " << width << 'x' << height << '\n';
    file.close();

    //=========================preparations-done==========================//

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        std::cout << "can`t open file\n";
        return -1;
    }
    uint32_t n_of_thrs;
    int filter_type;
    n_of_thrs = atoi(argv[2]);
    filter_type = atoi(argv[3]);
    if (n_of_thrs > height) n_of_thrs = height;
    std::thread threads[n_of_thrs];
    unsigned char* mapping = (unsigned char*)(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (mapping == MAP_FAILED) {
        std::cout << "mmap failed\n";
        return -1;
    }
    uint32_t pool = (height / n_of_thrs) * width * 3;
    uint32_t leftover = (height % n_of_thrs) * width * 3;
    uint32_t start = m_offset;
    uint32_t end = m_offset + pool;

    std::unique_lock<std::mutex> lock(mutex);
    if (filter_type == 1) {
        for (int i = 0; i < n_of_thrs; i++) {
            threads[i] = std::thread(reverse, std::ref(start), std::ref(end), mapping);
            cv.wait(lock);
            start += pool;
            if (i == n_of_thrs - 2) {
                end += pool + leftover;
            } else
                end += pool;
        }
    } else if (filter_type == 2) {
        for (int i = 0; i < n_of_thrs; i++) {
            threads[i] = std::thread(black_n_white, std::ref(start), std::ref(end), mapping);
            cv.wait(lock);
            start += pool;
            if (i == n_of_thrs - 2) {
                end += pool + leftover;
            } else
                end += pool;
        }
    }
    mutex.unlock();

    for (int i = 0; i < n_of_thrs; i++) {
        threads[i].join();
    }
    return 0;
}