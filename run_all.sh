# This test suite generates the stats in the README.

time ./run_BlindBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_BlindBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_BlindBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_SimpleBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_SimpleBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_SimpleBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_ValueBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_ValueBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_ValueBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_HolmesBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_HolmesBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_HolmesBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_SmartBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_SmartBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_SmartBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_InfoBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_InfoBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_InfoBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_CheatBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_CheatBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_CheatBot --quiet --players 4 --seed 1 --games 200000 --every 200000

time ./run_NewCheatBot --quiet --players 2 --seed 1 --games 200000 --every 200000
time ./run_NewCheatBot --quiet --players 3 --seed 1 --games 200000 --every 200000
time ./run_NewCheatBot --quiet --players 4 --seed 1 --games 200000 --every 200000
