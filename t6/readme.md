# [SOI] - Koncepcja lab 6 (t6)

## Treść zadania
(aplikacja w C++/Python itp symulująca system plików) - W pliku na dysku należy zorganizować system plików z wielopoziomowym katalogiem.
Należy zrealizować aplikację konsolową, przyjmującą polecenia, wywoływaną z nazwą pliku implementującego dysk wirtualny.

Należy zaimplementować następujące operacje, dostępne dla użytkownika tej aplikacji:
- tworzenie wirtualnego dysku (gdy plik wirtualnego dysku będący parametrem nie istnieje to pytamy się o utworzenie przed przejściem do interakcji) - jak odpowiedź negatywna to kończymy program. Parametrem polecenia powinien być rozmiar tworzonego systemu plików w bajtach. Dopuszcza się utworzenie systemu nieznacznie większego lub mniejszego, gdy wynika to z przyjętych założeń dotyczących budowy.
- kopiowanie pliku z dysku systemu na dysk wirtualny,
- utworzenie katalogu na dysku wirtualnym (katalogi mogą być zagnieżdżane - jednym poleceniem mkdir a/b/c tworzymy 3 katalogi)
- usunięcie katalogu z dysku wirtualnego
- kopiowanie pliku z dysku wirtualnego na dysk systemu,
- wyświetlanie katalogu dysku wirtualnego z informacją o rozmiarze (sumie) plików w katalogu, rozmiarze plików w katalogu razem z podkatalogami (suma), oraz ilości wolnej pamięci na dysku wirtualnym
- tworzenie twardego dowiązania do pliku lub katalogu
- usuwanie pliku lub dowiązania z wirtualnego dysku,
- dodanie do pliku o zadanej nazwie n bajtów
- skrócenie pliku o zadanej nazwie o n bajtów
- wyświetlenie informacji o zajętości dysku.

## Implementacja
1. Implementacja w języku `C/C++`

2. Utworzę system plików bardzo podobny do tego, który znajduje się w systemie Linux

3. Utworzę trzy struktury danych:
  - Blok Nadrzędny, który będzie "zarządzał" całym systemem, zawierał będzie takie informacje jak:
    - rozmiar wszystkich plików
    - informacja o ilości bloków i zajętości
    - informacje o pamięci itp.
    - informacje o INode'ach
    - wskaźnik na listę INode'ów

  - INode, który będzie zawierał informacje o pliku/katalogu, takie jak:
    - informacja o tym czy to jest plik czy może katalog
    - data utworzenia
    - rozmiar pliku/katalogu
    - wskaźnik na blok danych

  - Blok Danych
    - dane
    - wskaźnik na kolejny blok danych

4. I-Węzły nie będą rozpoznawanie po nazwie, tylko po ID. Nazwy plików będą zapisane w Bloku nadrzędnym. Będzie on zawierał listę z krotkami, które będą zawierać nazwę oraz id danego węzła. Dzięki temu będziemy mieli pewność co do unikalności węzłów.

5. Katalog i plik, będą tym samym węzłem, jednakże aby byłe możliwe rozróżnienie pomiędzy nimi, będę informacja w węźle informacja o typie.

6. INode będący katalogiem będzie zawierał wskaźnik do bloku danych, który to będzie przechowywał listę INodów.

7. Blok danych będzie miał będzie miał stały rozmiar. Dlatego aby móc tworzyć pliki o większym rozmiarze, trzeba będzie zaimplementować kolejne poziomu danych, dlatego w bloku danych będzie znajdował się wskaźnik, na kolejne dane.

8. Zaimplementuje wymagane funkcje, wymienione w treści zadania.

## Testowanie

Program będę testował poprzez wykonywanie odpowiednich operacji:
- usuwanie katalogu
- kopiowanie plików
- dodawanie plików
- wyświetlanie katalogu
- tworzenie dowiązania

Przed i po każdej operacji będę wyświetlał zawartość wszystkich katalogów, po to aby sprawdzić, czy wykonała się odpowiednia operacja, czy znajduje się odpowiednia ilość plików i czy są to poprawne pliki.


# Autor
Imię i Nazwisko: Mateusz Brzozowski\
Nr. Indeksu: 310608
