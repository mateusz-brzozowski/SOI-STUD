#!/bin/bash

set -e
number_of_test=$1
disc_name="test"

case $number_of_test in
    "1")
    echo "Creating disc with 2097152 size\n"
    ./a.out $disc_name create 2097152
    ./a.out $disc_name ls /
    ;;
    "2")
    echo "Creating directories\n"
    ./a.out $disc_name mkdir a/x/k
    ./a.out $disc_name mkdir a/x/l
    ./a.out $disc_name mkdir a/y
    ./a.out $disc_name mkdir b
    ./a.out $disc_name ls /
    ./a.out $disc_name ls a/x/l
    ./a.out $disc_name tree /
    ;;
    "3")
    echo "Coping small file from disc to virtual disc\n"
    ./a.out $disc_name ls a/x/l
    ./a.out $disc_name tree /
    ./a.out $disc_name send a/x/l matejko
    ./a.out $disc_name ls a/x/l
    ./a.out $disc_name tree /
    ;;
    "4")
    echo "Coping small file from virtual disc to disc\n"
    ./a.out $disc_name ls a/x/l
    ./a.out $disc_name tree /
    ./a.out $disc_name get a/x/l/matejko matejko_out
    ./a.out $disc_name ls a/x/l
    ./a.out $disc_name tree /
    diff matejko matejko_out
    ;;
    "5")
    echo "Coping large file from disc to virtual disc\n"
    ./a.out $disc_name ls /
    ./a.out $disc_name tree /
    ./a.out $disc_name send / tadek
    ./a.out $disc_name ls /
    ./a.out $disc_name tree /
    ;;
    "6")
    echo "Coping large file from virtual disc to disc\n"
    ./a.out $disc_name ls /
    ./a.out $disc_name tree /
    ./a.out $disc_name get tadek tadek_out
    ./a.out $disc_name ls /
    ./a.out $disc_name tree /
    diff matejko matejko_out
    diff tadek tadek_out
    ;;
    "7")
    echo "Extending matejko file size\n"
    ./a.out $disc_name ls a
    ./a.out $disc_name tree /
    ./a.out $disc_name extend a/x/l/matejko 25000
    ./a.out $disc_name ls a
    ./a.out $disc_name tree /
    ;;
    "8")
    echo "Truncating matejko file size\n"
    ./a.out $disc_name ls a
    ./a.out $disc_name tree /
    ./a.out $disc_name cut a/x/l/matejko 25000
    ./a.out $disc_name ls a
    ./a.out $disc_name tree /
    ;;
    "9")
    echo "Createing link\n"
    ./a.out $disc_name tree /
    ./a.out $disc_name ln tadek a/y/tadek_link
    ./a.out $disc_name ln a/x b/blink
    ./a.out $disc_name ls b
    ./a.out $disc_name tree /
    ;;
    "10")
    echo "Removing files\n"
    ./a.out $disc_name ls /
    ./a.out $disc_name tree /
    ./a.out $disc_name rm tadek
    ./a.out $disc_name ls /
    ./a.out $disc_name rm a/x/l/matejko
    ./a.out $disc_name rm a/y/tadek_link
    ./a.out $disc_name ls /
    ./a.out $disc_name tree /
    ;;
    *) echo "No test" ;;
esac