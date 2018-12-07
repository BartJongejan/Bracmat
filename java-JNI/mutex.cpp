// mutex::lock/unlock
#include <mutex>          // std::mutex

std::mutex mutexout;
std::mutex mutexinit;
std::mutex mutexend;

extern "C" void mutexLockOut()
    {
    mutexout.lock();
    }

extern "C" void mutexLockInit()
    {
    mutexinit.lock();
    }

extern "C" void mutexLockEnd()
    {
    mutexend.lock();
    }

extern "C" void mutexUnlockOut()
    {
    mutexout.unlock();
    }

extern "C" void mutexUnlockInit()
    {
    mutexinit.unlock();
    }

extern "C" void mutexUnlockEnd()
    {
    mutexend.unlock();
    }

