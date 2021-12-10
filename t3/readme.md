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

