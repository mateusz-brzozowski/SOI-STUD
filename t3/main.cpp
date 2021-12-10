// g++ main.cpp -lpthread

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "monitor.h"

#define SLEEP usleep(1000000 + rand() % 1000000)
#define PRINT(x) std::cout << x << std::endl

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

    int count() const{
        return buffer.size();
    }

    int countEven() const{
        int counter = 0;
        for(int i = 0; i < buffer.size(); i++)
            if(buffer[i] % 2 == 0)
                counter++;
        return counter;
    }

    int countOdd() const{
        return buffer.size() - countEven();
    }

    int check() const{
        if(buffer.empty())
            return -1;
        return buffer[0];
    }
};

int num_of_prod_even_waiting = 0;
int num_of_prod_odd_waiting = 0;
int num_of_cons_even_waiting = 0;
int num_of_cons_odd_waiting = 0;

Semaphore mutex = Semaphore(1);
Semaphore prod_even_mutex = Semaphore(0);
Semaphore prod_odd_mutex = Semaphore(0);
Semaphore cons_even_mutex = Semaphore(0);
Semaphore cons_odd_mutex = Semaphore(0);

Buffer my_buffer;

bool canProdEven(){
    if (my_buffer.countEven() < 10)
        return true;
    return false;
}

bool canProdOdd(){
    if (my_buffer.countEven() > my_buffer.countOdd())
        return true;
    return false;
}

bool canConsEven(){
    if(my_buffer.count() >= 3 && my_buffer.check() != -1 && my_buffer.check() % 2 == 0)
        return true;
    return false;
}

bool canConsOdd(){
    if(my_buffer.count() >= 7 && my_buffer.check() != -1 && my_buffer.check() % 2 != 0)
        return true;
    return false;
}

int generateEvenNumber(){
    return (rand() % (25 + 1)) * 2;
}

int generateOddNumber(){
    return (rand() % (24 + 1)) * 2 + 1;
}

void* prodEven(void* arg){
    while (1){
        mutex.p();
        if(!canProdEven()){
            ++num_of_prod_even_waiting;
            mutex.v();
            prod_even_mutex.p();
            --num_of_prod_even_waiting;
        }
        int even_number = generateEvenNumber();
        my_buffer.put(even_number);
        std::string s = "A1: Dodano " + std::to_string(even_number);
        PRINT(s);
        if(num_of_prod_odd_waiting > 0 && canProdOdd())
            prod_odd_mutex.v();
        else if(num_of_cons_even_waiting > 0 && canConsEven())
            cons_even_mutex.v();
        else if(num_of_cons_odd_waiting > 0 && canConsOdd())
           cons_odd_mutex.v();
        else
            mutex.v();
        SLEEP;
    }
}

void* prodOdd(void* arg){
    while (1){
        mutex.p();
        if(!canProdOdd()){
            ++num_of_prod_odd_waiting;
            mutex.v();
            prod_odd_mutex.p();
            --num_of_prod_odd_waiting;
        }
        int odd_number = generateOddNumber();
        my_buffer.put(odd_number);
        std::string s = "A2: Dodano " + std::to_string(odd_number);
        PRINT(s);
        if(num_of_prod_even_waiting > 0 && canProdEven())
            prod_even_mutex.v();
        else if(num_of_cons_even_waiting > 0 && canConsEven())
            cons_even_mutex.v();
        else if(num_of_cons_odd_waiting > 0 && canConsOdd())
           cons_odd_mutex.v();
        else
            mutex.v();
        SLEEP;
    }
}

void* consEven(void* arg){
    while (1){
        mutex.p();
        if(!canConsEven()){
            ++num_of_cons_even_waiting;
            mutex.v();
            cons_even_mutex.p();
            --num_of_cons_even_waiting;
        }
        int value = my_buffer.get();
        std::string s = "B1: Usunieto " + std::to_string(value);
        PRINT(s);
        if(num_of_cons_odd_waiting > 0 && canConsOdd())
            cons_odd_mutex.v();
        else if(num_of_prod_even_waiting > 0 && canProdEven())
            prod_even_mutex.v();
        else if(num_of_prod_odd_waiting > 0 && canProdOdd())
            prod_odd_mutex.v();
        else
            mutex.v();
        SLEEP;
    }
}

void* consOdd(void* arg){
    while (1){
        mutex.p();
        if(!canConsOdd()){
            ++num_of_cons_odd_waiting;
            mutex.v();
            cons_odd_mutex.p();
            --num_of_cons_odd_waiting;
        }
        int value = my_buffer.get();
        std::string s = "B2: Usunieto " + std::to_string(value);
        PRINT(s);
        if(num_of_cons_even_waiting > 0 && canConsEven())
            cons_even_mutex.v();
        else if(num_of_prod_even_waiting > 0 && canProdEven())
            prod_even_mutex.v();
        else if(num_of_prod_odd_waiting > 0 && canProdOdd())
            prod_odd_mutex.v();
        else
            mutex.v();
        SLEEP;
    }
}

int main(int argc, char* argv[]){
    if (argc != 2){
        std::cout << "Invalid number of arguments!\nTry: "<< argv[0] <<" [number_of_test]\n";
        return 0;
    }
    srand(time(NULL));
    pthread_t th[4];
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
        // TUTAJ TRZEBA DODAĆ JAKIEŚ BLOKOWANIE!!
        pthread_create(&th[0], NULL, &prodEven, NULL);
        pthread_create(&th[1], NULL, &prodOdd, NULL);
        pthread_create(&th[2], NULL, &consEven, NULL);
        pthread_create(&th[3], NULL, &consOdd, NULL);
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

