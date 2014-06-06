Stout - stability tester
========================

Stout is a simple tool to check performance stability of long running 
applications. E.g. it can be used to verify that a server application doesn't
leak memory, or that it never exceed specified CPU usage.

Usage
-----


~~~
stout.exe -i <file path> [--version] [-h]
~~~
 
Parameters:

~~~
   -i <file path>,  --ini <file path>
     (required)  Location of ini file containing test configuration

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.
~~~

Example:

~~~
# run test configured in d:\my_test.ini
stout.exe -i d:\my_test.ini
~~~


Configuration
-------------

Tests are configured through simple ini file which specifies which processes are
to be executed, which counters are to be monitored, and which limits are to be
checked.

Stout starts the tested application and then lets them run for some time to 
collect baseline data. This is controlled by DELAY and SAMPLING_TIME parameters.

Then it continues to monitor the running application and compares the collected
metrics against the baseline data.

Example:

~~~{.ini}
[STOUT::COMMON]      ; Common parameters 
DELAY = 5            ; Initial delay in seconds. When the tested apps are
                     ; started, baseline assessment begins after the delay 
                     ; expires. If ommitted, default is 5000 (5 s)
SAMPLING_TIME = 60   ; How often will counters be averaged and evaluated, in 
                     ; seconds. This is also the duration of baseline assessment
                     ; If ommitted, 60 s is used as default
ON_ERROR = LOG       ; LOG | STOP - how to react if limits are violated. If LOG
                     ; is specified, violation will be logged and test will
                     ; continue. If STOP is specified, test will be aborted.
                     ; LOG is the default value. 
DURATION = 60        ; How long will test be executed, in minutes. When this 
                     ; period expires, test will be stopped. If set to 0, test
                     ; will run until manually stopped. Default is 60 s
                     
; metrics which are required for all tested apps are specified here

METRIC = MEM.WS < 1% ; Working set for all apps must not increase more than 1%
METRIC = MEM.PB < 1% ; Private bytes for all apps must not increase more than 1%


[STOUT::BACKENDS]         ; collected data are written to specified backends
GRAPHITE=graphitesvr:9924 ; send to graphite
CSV="path\file.csv"       ; write to CSV file

; now we configure tested apps

[consumer]            ; each app has a symbolic name - consumer, in this case
START = consumer.exe  ; start consumer.exe
COUNT = 3             ; run 3 instances. if ommitted, default is 1
METRIC = MEM.WS < 1%  ; working set must be constant
~~~

[producer]            ; run the producer
ATTACH = producer.exe ; don't start it - attach to an existing instance 
                      ; In this case, COUNT is attached.
                      ; if multiple instances are running, process id can be
                      ; specified, e.g. 'producer.exe:5624'
METRIC = CPU < 50     ; CPU usage must not exceed 50%
METRIC = MEM.WS < 1%  ; working set must be constant


Supported metrics
-----------------

Name     | Description
:--------|----------------------------------------------------------
MEM.WS   | Working set size. If using absolute limit, it is specified in kB    
MEM.PB   | Private bytes. If using absolute limit, it is specified in kB        
CPU      | CPU usage. if using absolute limit, it is specified in %            


Logging and backends
--------------------

Collected data can be logged. Different backends can be used (file, console...)
This is specified in `STOUT::BACKENDS` section of configuration file.

