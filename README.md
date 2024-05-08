# You Spin Me Round Robin

This program simulates the implementation of Round-robin style scheduling and 
provides corresponding average wait and response times given a pre-determined completion
and arrival timeframe of the processes at hand. 

## Building

```shell
make
```
This will build the executable rr

## Running

processes.txt takes the form 
```
4 (num of processes)
1, 0, 7 
2, 2, 4
3, 4, 1 (pid, arrival, burst)
4, 5, 4
```
cmd for running processes.txt with a time-slice of 3
```shell
./rr processes.txt 3
```

results 
```shell
Average waiting time: 7.00
Average response time: 2.75
```

## Cleaning up

```shell
make clean
```
will remove the binaries made during the build process
