// g++ main.cpp -lpthread

#include <iostream>
#include <vector>
#include <pthread.h>
#include <unistd.h>

#include "monitor.h"

#pragma region Buffer

class Buffer {
    std::vector<int> buffer;
public:
    void put(int value){
        buffer.push_back(value);
    }

    int get(){
        if(buffer.empty())
            return -1;
        int value = buffer[0];
        buffer.erase(buffer.begin());
        return value;
    }

    int get_first() const{
        if(buffer.empty())
            return -1;
        return buffer[0];
    }

    int count() const{
        return buffer.size();
    }

    int countEven() const{
        int even_values = 0;
        for(int i = 0; i < buffer.size(); i++)
            if(buffer[i] % 2 == 0)
                even_values++;
        return even_values;
    }

    int countOdd() const{
        int odd_values = 0;
        for(int i = 0; i < buffer.size(); i++)
            if(buffer[i] % 2 == 1)
                odd_values++;
        return odd_values;
    }
};

#pragma endregion Buffer

#pragma region Monitor

class MyMonitor : Monitor {
    Buffer buffer;
    Condition prodEvenCond, prodOddCond, consEvenCond, consOddCond;
    unsigned int num_of_prod_even_waiting = 0;
    unsigned int num_of_prod_odd_waiting = 0;
    unsigned int num_of_cons_even_waiting = 0;
    unsigned int num_of_cons_odd_waiting = 0;
    bool canProdEven(), canProdOdd(), canConsEven(), canConsOdd();

public:
    void putEven(unsigned int value);
    void putOdd(unsigned int value);
    unsigned int getEven(bool lock);
    unsigned int getOdd(bool lock);
};

void MyMonitor::putEven(unsigned int value){
    enter();
    if(!canProdEven()){
        num_of_prod_even_waiting++;
        leave();
        wait(prodEvenCond);
        num_of_prod_even_waiting--;
    }
    buffer.put(value);
    std::cout << "A1(" << buffer.countEven() << "): Dodano " << std::to_string(value) << std::endl;
    if (num_of_prod_odd_waiting > 0 && canProdOdd())
        signal(prodOddCond);
    else if (num_of_cons_even_waiting > 0 && canProdEven())
        signal(consEvenCond);
    else if (num_of_cons_odd_waiting > 0 && canConsOdd())
        signal(consOddCond);
    else
        leave();
}

void MyMonitor::putOdd(unsigned int value){
    enter();
    if(!canProdOdd()){
        num_of_prod_odd_waiting++;
        leave();
        wait(prodOddCond);
        num_of_prod_odd_waiting--;
    }
    buffer.put(value);
    std::cout << "A2(" << buffer.countOdd() << "): Dodano " << std::to_string(value) << std::endl;
    if (num_of_prod_even_waiting > 0 && canProdEven())
        signal(prodEvenCond);
    else if (num_of_cons_even_waiting > 0 && canProdEven())
        signal(consEvenCond);
    else if (num_of_cons_odd_waiting > 0 && canConsOdd())
        signal(consOddCond);
    else
        leave();
}

unsigned int MyMonitor::getEven(bool lock){
    enter();
    if(!canConsEven()){
        num_of_cons_even_waiting++;
        if(!lock)
            leave();
        wait(consEvenCond);
        num_of_cons_even_waiting--;
    }
    int value = buffer.get();
    std::cout << "B1: Usunieto " << std::to_string(value) << std::endl;
    if (num_of_prod_even_waiting > 0 && canProdEven())
        signal(prodEvenCond);
    else if (num_of_prod_odd_waiting > 0 && canProdOdd())
        signal(prodOddCond);
    else if (num_of_cons_odd_waiting > 0 && canConsOdd())
        signal(consOddCond);
    else
        leave();
    return value;
}

unsigned int MyMonitor::getOdd(bool lock){
    enter();
    if(!canConsOdd()){
        num_of_cons_odd_waiting++;
        if (!lock)
            leave();
        wait(consOddCond);
        num_of_cons_odd_waiting--;
    }
    int value = buffer.get();
    std::cout << "B2: Usunieto " << std::to_string(value) << std::endl;
    if (num_of_prod_even_waiting > 0 && canProdEven())
        signal(prodEvenCond);
    else if (num_of_prod_odd_waiting > 0 && canProdOdd())
        signal(prodOddCond);
    else if (num_of_prod_odd_waiting > 0 && canProdOdd())
        signal(prodOddCond);
    else
        leave();
    return value;
}

bool MyMonitor::canProdEven(){
    if (buffer.countEven() < 10)
        return true;
    return false;
}

bool MyMonitor::canProdOdd(){
    if (buffer.countEven() > buffer.countOdd())
        return true;
    return false;
}

bool MyMonitor::canConsEven(){
    if(buffer.count() >= 3 && buffer.get_first() != -1 && buffer.get_first() % 2 == 0)
        return true;
    return false;
}

bool MyMonitor::canConsOdd(){
    if(buffer.count() >= 7 && buffer.get_first() != -1 && buffer.get_first() % 2 != 0)
        return true;
    return false;
}

#pragma endregion Monitor

#pragma region MainFunctions

MyMonitor monitor;

void* prodEven(void* arg){
    for(int even_number = 0;; even_number = (even_number +2) % 50){
        monitor.putEven(even_number);
        usleep(1000000 + rand() % 1000000);
    }
}

void* prodOdd(void* arg){
    for(int odd_number = 1;; odd_number = (odd_number + 2) % 50){
        monitor.putOdd(odd_number);
        usleep(1000000 + rand() % 1000000);
    }
}

void* consEven(void* arg){
    bool lock = false;
    if(arg != NULL)
       lock = *(bool*)arg;
    while (1){
        monitor.getEven(lock);
        usleep(1000000 + rand() % 1000000);
    }
}

void* consOdd(void* arg){
    bool lock = false;
    if(arg != NULL)
       lock = *(bool*)arg;
    while (1){
        monitor.getOdd(lock);
        usleep(1000000 + rand() % 1000000);
    }
}

#pragma endregion MainFunctions

int main(int argc, char* argv[]){
    if (argc != 2){
        std::cout << "Invalid number of arguments!\nTry: "<< argv[0] <<" [number_of_test]\n";
        return 0;
    }
    srand(time(NULL));
    pthread_t th[4];
    bool lock = true;
    switch (*argv[1])
    {
    case '0':
        pthread_create(&th[0], NULL, &prodEven, NULL);
        pthread_join(th[0], NULL);
        break;
    case '1':
        pthread_create(&th[0], NULL, &prodOdd, NULL);
        pthread_join(th[0], NULL);
        break;
    case '2':
        pthread_create(&th[0], NULL, &consEven, NULL);
        pthread_join(th[0], NULL);
        break;
    case '3':
        pthread_create(&th[0], NULL, &consOdd, NULL);
        pthread_join(th[0], NULL);
        break;
    case '4':
        pthread_create(&th[0], NULL, &prodEven, NULL);
        pthread_create(&th[1], NULL, &prodOdd, NULL);
        pthread_join(th[0], NULL);
        pthread_join(th[1], NULL);
        break;
    case '5':
        pthread_create(&th[0], NULL, &consEven, NULL);
        pthread_create(&th[1], NULL, &consOdd, NULL);
        pthread_join(th[0], NULL);
        pthread_join(th[1], NULL);
        break;
    case '6':
        pthread_create(&th[0], NULL, &prodEven, NULL);
        pthread_create(&th[1], NULL, &prodOdd, NULL);
        pthread_create(&th[2], NULL, &consEven, NULL);
        pthread_create(&th[3], NULL, &consOdd, NULL);
        pthread_join(th[0], NULL);
        pthread_join(th[1], NULL);
        pthread_join(th[2], NULL);
        pthread_join(th[3], NULL);
        break;
    case '7':
        pthread_create(&th[0], NULL, &prodEven, NULL);
        pthread_create(&th[1], NULL, &prodOdd, NULL);
        pthread_create(&th[2], NULL, &consEven, (void*) &lock);
        pthread_create(&th[3], NULL, &consOdd, (void*) &lock);
        pthread_join(th[0], NULL);
        pthread_join(th[1], NULL);
        pthread_join(th[2], NULL);
        pthread_join(th[3], NULL);
        break;
    default:
        pthread_create(&th[0], NULL, &prodEven, NULL);
        pthread_join(th[0], NULL);
        break;
    }
    return 0;
}
