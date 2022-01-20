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

<h2 id="zalozenia">Założenia</h2>


## Implementacja
1. Implementacja w języku `C/C++`



## Testowanie


## Scenariusze testowe

# Autor
Imię i Nazwisko: Mateusz Brzozowski\
Nr. Indeksu: XXXXXXX
