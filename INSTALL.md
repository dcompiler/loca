-- To install on Mac --
1. You need to disable SIP. You can do this by going to Recovery Mode in Mac, by restarting your computer holding down Command+R.
   a. After starting in Recovery Mode, Click Utilities and Terminal.
   b. In the terminal, type the command 'csrutil disable'
   c. Restart the computer, SIP is disabled.
   EXTRA: If you still recieve an error about task_for_pid, Make sure
   to configure PIN by running their host_config (located in
   pin/sbin/host_config)

2. GCC vs LLVM

The LLVM compiler installed on Mac machines is sufficient.
