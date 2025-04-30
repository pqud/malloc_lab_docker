# Implicit Next
getopt returned: 118
Team Name:Team6
Member 1 :Hyeonho Cho:joho0504@gmail.com
Member 2 :Harin Lee:gbs1823@gmail.com
Member 3 :Eunchae Park:ghkqh09@gmail.com
Using default tracefiles in ./traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   91%    5694  0.001048  5433
 1       yes   91%    5848  0.000634  9228
 2       yes   95%    6648  0.002265  2935
 3       yes   97%    5380  0.002450  2196
 4       yes   66%   14400  0.000076189723
 5       yes   90%    4800  0.002460  1951
 6       yes   88%    4800  0.002045  2347
 7       yes   55%   12000  0.009682  1239
 8       yes   51%   24000  0.004784  5017
 9       yes   27%   14401  0.323274    45
10       yes   45%   14401  0.016826   856
Total          72%  112372  0.365543   307

Perf index = 43 (util) + 20 (thru) = 64/100
[1] + Done                       "/usr/bin/gdb" --interpreter=mi --tty=${DbgTerm} 0<"/tmp/Microsoft-MIEngine-In-uiv1wavm.yx0" 1>"/tmp/Microsoft-MIEngine-Out-q012gljz.5wi"

# Explicit LIFO

getopt returned: 118
Team Name:Team6
Member 1 :Hyeonho Cho:joho0504@gmail.com
Member 2 :Harin Lee:gbs1823@gmail.com
Member 3 :Eunchae Park:ghkqh09@gmail.com
Using default tracefiles in ./traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   88%    5694  0.000237 23995
 1       yes   91%    5848  0.000313 18684
 2       yes   94%    6648  0.000600 11082
 3       yes   96%    5380  0.000477 11269
 4       yes   66%   14400  0.000410 35088
 5       yes   88%    4800  0.000476 10080
 6       yes   85%    4800  0.000877  5476
 7       yes   54%   12000  0.005371  2234
 8       yes   47%   24000  0.005194  4621
 9       yes   26%   14401  0.316811    45
10       yes   34%   14401  0.018405   782
Total          70%  112372  0.349171   322

Perf index = 42 (util) + 21 (thru) = 63/100