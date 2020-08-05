# dktestrunner
Testrunner for KeeperFx

This tool should allow to make some debugging and autotests.

```
Usage: testrunner
        --exit_at_turn <num>            - exit level at specific time (default 12000 turns)
        --frameskip <num>               - set frameskip (default: 2)
        --read                  - just wait for events and print them
        --show_events           - print events while running tests
        --skip_events <regexp>  - ignore events that match regexp
```
