# UDPDataTransfer
TCP like service over UDP:
transport-layer service over UDP that can sustain non-trivial transfer
rates (hundreds of megabits/sec) under low packet loss and avoid getting bogged down under
heavy loss. 

How to run:
rdt.exe s3.irl.cs.tamu.edu 24 50000 0.2 0.00001 0.0001 100
Parameter:
s3.irl.cs.tamu.edu -> destination, server (hostname or IP)
24 -> a power-of-2 buffer size to be transmitted (in DWORDs)
50000 -> sender window(in packets)
0.2 -> the round-trip propagation delay (in seconds)
0.00001, 0.0001 -> the probability of loss in each direction
100 -> the speed of the bottleneck link (in Mbps).
