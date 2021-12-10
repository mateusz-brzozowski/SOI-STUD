# [SOI] - Koncepcja lab 3 (t3)

## Treść zadania
Mamy bufor FIFO na liczby całkowite. Procesy A1 generują kolejne liczby parzyste modulo 50,
jeżeli w buforze jest mniej niż 10 liczb parzystych. Procesy A2 generują kolejne liczby
nieparzyste modulo 50, jeżeli liczb parzystych w buforze jest więcej niż nieparzystych. Procesy
B1 zjadają liczby parzyste pod warunkiem, że bufor zawiera co najmniej 3 liczby. Procesy B2
zjadają liczby nieparzyste, pod warunkiem, że bufor zawiera co najmniej 7 liczb. W systemie
może być dowolna liczba procesów każdego z typów. Zrealizuj wyżej wymienioną
funkcjonalność przy pomocy semaforów. Zakładamy, że bufor FIFO poza standardowym put()
i get() ma tylko metodę umożliwiającą sprawdzenie liczby na wyjściu (bez wyjmowania) oraz
posiada metody zliczające elementy parzyste i nieparzyste. Zakładamy, że semafory mają tylko
operacje P i V.

<h2 id="zalozenia">Założenia</h2>

1. Podział procesów:
    - `A1` - generowanie liczb parzystych modulo 50, jeżeli < 10 liczb parzystych
    - `A2` - generowanie liczb nieparzystych modulo 50, jeżeli parzystych > nieparzystych
    - `B1` - zjadają liczby parzyste, jeżeli >= 3 liczby
    - `B2` - zjadają liczby nieparzyste, jeżeli >= 7 liczb.

2. bufor FIFO ma cztery metody:
    - wkładamy liczbę
    - bierzemy liczbę
    - sprawdzamy liczby na wyjściu
    - zliczamy elementy parzyste i nieparzyste

## Implementacja

1. Implementacja w języku `C/C++`

2. Tworzymy pięć semaforów, które podzielimy na dwie grupy i przypiszemy odpowiednim grupom inne zadania:
    - `1` będzie odpowiedzialny za ochronę buforów przed niewłaściwym użyciem pozostałych buforów. Jeżeli semafor `1` zablokowany, to jedynie proces, który posiada tę blokadę może operować na buforze, w przeciwnym wypadku nie można wykonywać żadnych operacji.
    - `2`, `3`, `4`, `5` będą obsługiwały podział procesów (generowanie i zjadanie liczb) zgodnie z [założeniami](#zalozenia). Jednakże ich działanie będzie dodatkowo  ograniczone (poza ograniczeniem `1` semafora), ponieważ będziemy wybierali semafory ale w określonej kolejności, tak aby w danej chwili mógł być odblokowany tylko jeden semafor.

## Testowanie

1. Dla każdej operacji dodamy w kodzie odpowiednie funkcje testujące, które będą weryfikowały, czy w danym momencie odpowiedni semafor jest odblokowany, jeżeli tak nie będzie użytkownik będzie poinformowany odpowiednim komunikatem.

2. Kolejnym testem będzie odpowiednie spowolnienie działania programu i wyświetlanie użytkownikowi stanu bufora po każdorazowej zmianie stanu, po to aby sprawdzić, czy wykonywane są poprawne operacje.

# Autor
Imię i Nazwisko: Mateusz Brzozowski\
Nr. Indeksu: XXXXXXX

# Przykładowe rozwiązanie zadania
## Założenia
- `5` semaforów <b>binarnych</b>
    - jeden bufor ogólny:
        - `mutex(1)`
    - każdy dla każdej grupy (konsument parzysty/nieparzysty):
        - `prodEven(0)`
        - `prodOdd(0)`
        - `consEven(0)`
        - `consOdd(0)`
- `4` zmiennne globalne

## Implementaja
```c
//Kompilowanie:  g++ main.cpp -lpthread

#include "monitor.h"

unsigned int numOfProdEvenWating =  0;
unsigned int numOfPordOddWating = 0;
unsigned int numOfConsEvenWating =  0
unsigned int numOfConsOddWating = 0;

void prodEven(){
    while(1){
        mutex.P();
        if(!canProdEven()){
            numOfProdEvenWating++;
            mutex.V();          // Do zakomentowania do testów żeby się zakleszczyło!
            prodEven.P();
            numOfProdEvenWating--;
        }
        buffer.push(generateEvenNumber());

        if(canProdOdd() && numOfProdOddWating){
            prodOdd.V();
        }
        else if(canConsEven && numOfConsEvenWating){
            consEven.V();
        }
        else if(canConsOdd && numOfConsOddWating){
            consOdd.V();
        }
        else{
            mutex.V();
        }

        sleep(rand + CONST)
    }
}

int main(){
    //  po dwie sekcja:
    pthread_create(&);
    pthread_join(th[0], NULL);
}
```

## Scenariusze testowe
- sam producent parzysty:\
uzupłeni `10` procesów `A1: Dodano 30...` i  następnie się zatrzymuje
- sam producent nieparzsty:\
nic nie powinno się stać
- sam konsument parzysty:\
nic nie powinno się stać
- sam konstument nieparzysty:\
nic nie powinno się stać
- sami producenci:\
Na zmianę dodają sie parzyste i nieparzyste, `A1 - 10`, `A2 - 9`, łączenie 19 `A1: Dodano 20 A2: Dodano 9`
- sami konsumenci:\
nic nie powinno się stać
- normalny test, ze wszystkimi:\
ma po psrostu działać
- przykład zakleszczenia:
    - `mutex(0)`\
    lub
    - zakomentować `mutex.v()` w każdym semaforze\
`A1: Dodano 48`\
`A2: Dodano 11`\
i tyle. stop (zakleszczenie)

<b>MA BYĆ 8 SCENARIUSZY TESTOWYCH</b>