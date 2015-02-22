# wav-dump
A small utility for generating audio signals from sums of sinusoidal signals of different frequencies.

Type "make" in order to build the binary. Should work out of the box on any Unix like OS.

Syntax  : wavdump
	  [output file name]
	  [output file duration (greater than 0)]
	  [list of frequencies (values in the range [20 - 22k])]

Usage   : wavdump test.wav 5 440 880
Usage   : wavdump a.wav 10 1000 2000 3000
