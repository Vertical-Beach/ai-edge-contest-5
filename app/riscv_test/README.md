# setup cross-compile
TODO

# cross-compile
```sh
cd host
export PATH=$PATH:~/x-tools/riscv32-unknown-elf/bin/
make
cp riscv_imm.c ../edge
```

# test riscv
Copy application to ultra96.
```sh
scp -r riscv_test root@192.168.xx.xx:/home/root
```
Compile test application and run.
```
cd riscv_test/edge
sh compile.sh
./a.out
```
```
n: 30
set input:433[microsec]
      run:873[microsec]
Attempt 0 Success
n: 30
set input:115[microsec]
      run:854[microsec]
Attempt 1 Success
n: 31
set input:117[microsec]
      run:967[microsec]
Attempt 2 Success
n: 30
set input:110[microsec]
      run:860[microsec]
Attempt 3 Success
...
Attempt 606 Success
ALL PASS