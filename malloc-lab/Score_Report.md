# Implicit First
getopt returned: 118
Team Name:Team6
Member 1 :Hyeonho Cho:joho0504@gmail.com
Member 2 :Harin Lee:gbs1823@gmail.com
Member 3 :Eunchae Park:ghkqh09@gmail.com
Using default tracefiles in ./traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.004453  1279
 1       yes   99%    5848  0.003587  1631
 2       yes   99%    6648  0.006519  1020
 3       yes  100%    5380  0.004225  1273
 4       yes   66%   14400  0.000064226060
 5       yes   92%    4800  0.003544  1354
 6       yes   92%    4800  0.003219  1491
 7       yes   55%   12000  0.088642   135
 8       yes   51%   24000  0.161178   149
 9       yes   27%   14401  0.311348    46
10       yes   34%   14401  0.016351   881
Total          74%  112372  0.603130   186

Perf index = 44 (util) + 12 (thru) = 57/100

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
 0       yes   88%    5694  0.000238 23965
 1       yes   91%    5848  0.000200 29255
 2       yes   94%    6648  0.000338 19674
 3       yes   96%    5380  0.000249 21624
 4       yes   66%   14400  0.000307 46860
 5       yes   88%    4800  0.000560  8567
 6       yes   85%    4800  0.000526  9122
 7       yes   54%   12000  0.005027  2387
 8       yes   47%   24000  0.005612  4276
 9       yes   26%   14401  0.307865    47
10       yes   34%   14401  0.015998   900
Total          70%  112372  0.336920   334

Perf index = 42 (util) + 22 (thru) = 64/100