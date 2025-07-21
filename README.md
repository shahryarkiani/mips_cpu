
You'll need to install `mipsel-linux-gnu-gcc`, specifically version `10.3.0`


Run `make all` to build the simulator and the benchmarks.

# Run the simulator
`./processor --bmk=<path-to-benchmark-executable> -O<opt-level> > log`

The optimization levels are: 


    0 - Single cycle processor, used as reference for correct final register state
    1 - Pipelined Processor, 5 stage pipeline, F D E M W
    2 - Superscalar Processor, still 5 stages, but 2 of each stage
    3 - Perceptron Branch Prediction, built on top of superscalar processor. 
