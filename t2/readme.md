# [SOI] - Koncepcja lab 2 (t2)

## Treść zadania
Proszę zrealizować algorytm szeregowania dzielący procesy użytkownika na dwie grupy: A i B. Dodatkowo,
proszę opracować funkcję systemową umożliwiającą przenoszenie procesów pomiędzy grupami. Procesy w
grupie B otrzymują dwa razy więcej czasu niż procesy z grupy A.
Zakładamy, że nowy proces domyślnie znajduje się w grupie A oraz że w grupie A znajduje się co najmniej 1
proces. Opracować również łatwą metodę weryfikacji poprawności rozwiązania.

## Założenia
1. Przydzielony kwant czasu dla poszczególnych grup procesów:
    - `B` - dwa razy więcej niż w grupie A
    - `C` - 0
    W tym celu będziemy potrzebowali zmiennej `p_count` zliczającej kwant czasu dla procesu.
2. Należy zaktualizować strukturę proc w celu stworzenia "flag" dla poszczególnych procesów, dzięki
czemu będziemy mogli przypisywać poszczególne procesy do konkretnych grup. Tak więc, dodajemy
zmienną `p_group`, a jej wartości dla poszczególnych grup wyglądają następująco:
    - `0` - grupa `A`
    - `1` - grupa `B`

3. Nowo utworzone procesy trafiają do grupy `A`, a więc zgodnie z pkt. 2 zmienna `p_group` otrzyma
wartość `0`.
4. W grupie `A` znajduje się co najmniej `1` proces.

## Algorytm szeregowania
W pliku /usr/src/kernel/proc.c modyfikujemy funkcję sched, będzie rozporządzała kwantami czasu, dla
poszczególnych grup, tak aby procesy z grupy B wykonywały się 2 razy częściej niż z grupy A.

- Domyślnie proces trafia do grupy `A`.
- Jeżeli `p_cout` < `p_group`:
    - proces trafia na początek kolejki
    - `p_cout++`
- W przeciwnym wypadku:
    - proces trafia na koniec (przekroczył on przydzielony dla niego kwant czasu)
    - `p_cout = 0`

Dzięki zastosowaniu tego algorytmu będziemy mogli rozporządzać tym jaki kwant czasu wykorzystuje proces
z danej grupy.

## Przenoszenie procesów pomiędzy grupami
Przenoszenie do grupy `B` i `A` odbywa się przez użytkownika. W tym celu podobnie jak na laboratorium 1 zostanie
utworzona funkcja `change_p_group`.

## Weryfikacja poprawności rozwiązaniahas
- Przenoszenie procesów pomiędzy grupami, możemy weryfikować poprzez wciśniecie na klawiaturze
klawisza `F1` i przeanalizowaniu tablicy procesów. Będziemy wtedy mogli zweryfikować, czy procesy
zmieniają się w tabeli.
- Algorytm szeregowania, zweryfikujemy poprzez napisanie odpowiedniej funkcji w języku C (podobnie
jak na laboratorium 1), która będzie mierzyła czas wykonania się procesu, dzięki czemu sprawdzimy czy
procesy z grupy `B` mają dwa razy więcej czasu niż procesy z grupy `A`.

# Rozwiązanie

## Zmodyfikowane pliki
- `/include/minix/callnr.h` dodajemy dwa wywołania systemowe `GET_P_PROUP` oraz `SET_P_GROUP` oraz zwiększamy liczbę wywołań systemowych `NCALLS` o `2`.
- `/include/minix/com.h` dodajemy syscole `SYS_GETPGROUP` oraz `SYS_SETPGROUP`.

- `src/fs/table.c` dodajemy `no_sys` dla dodanych wywołań systemowych.

- `src/mm/proto.h` dodajemy prototypy funkcji `get_p_group` oraz `set_p_group`.
- `src/mm/table.c` dodajemy funckje do tablicy `get_p_group` oraz `set_p_group`.
- `src/mm/main.c` implementujemy dwie funkcje `get_p_group`, która zwraca grupę procesu oraz `set_p_group`, która ustawia grupę procesowi.

- `src/kernel/proc.h` dodajemy zmienną `p_group` która zawiera grupę procesu.
- `src/kernel/system.c` 
    - na początku przekazyjemy prototypy funkcji `get_p_group` oraz `set_p_group`.
    - modyfikujemy funkcję `do_fork` ustawiamy zmienną `p_group` na wartość 0, dzięki temu proces będzie miał domyślną grupę `A`.
    - modyfikujemy funkcję `sys_task` dodajemy syscall'e do switcha.
    - implementujemy dwie funkcje `get_p_group`, która zwraca grupę procesu oraz `set_p_group`, która ustawia grupę procesowi.
- `src/kernel/dmp.c` dodajemy możliwość wyświetlania grupy w tablicy procesów `F1`. Zmieniamy nagłówek oraz dodajemy pole `g_group`.
- `src/kernel/clock.c` modyfikujemy funckję `do_clock` tak aby procesy z grupy `B` wykonywały się `2` razy częściej.

## Bootowanie
W katalogu `/usr/src/tools` uruchamiamy `make hdboot`, `cd`, `shutdown`, `boot`.

## Testowanie
- `/usr/test_set.c` funckja testuje ustawianie grupy
- `/usr/test_time.c` funckja testuje czas wykonywania procesu

# Autor
Imię i Nazwisko: Mateusz Brzozowski
Nr. Indeksu: XXXXXX
