### A simple NTP based clock for M5 Paper
```
https://docs.m5stack.com/en/core/m5paper_v1.1
```

1. Upon first boot, we connect to a configured Wi-Fi network as in @config.h
2. Contact an NTP server as per @config.h and sets the Real Time Clock (RTC)
3. Displays the retrieved time in hh:mm format
4. Sleeps the e-ink display for the remaining seconds of the current minute
5. Wakes up and checks the RTC for a valid time
6. Displays RTC if valid, else repeats from Step 2
