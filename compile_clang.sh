set -e
clang++ -o sdffont -g -O3 -m64 -Wall -Isource source/main.cpp
clang++ -o angelcode2font -g -O3 -m64 -Wall source/angelcode.cpp
