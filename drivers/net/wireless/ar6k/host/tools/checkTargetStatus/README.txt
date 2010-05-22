
checkTargetStatus:
    Objective:
        To detect target failure and reset the target.

    How is it done:
        In this implementation, we periodically check the transmit que
        status on host. There are three counters for each queue indicating
        the status:
           tx_attempt: Indicates the number of buffers, the driver
                        attempted to post. Not all of them may succeed
                        due to queue capacity.
           tx_post: Indicates the number of buffers, the driver was
                        successful in posting to HTC layer.
                        Difference between tx_attempt and tx_post is
                        the number of packets dropped at host by the 
                        driver.
           tx_complete: Indicates the number of packets HTC was able
                        to transmit over the underlying hardware.

            
           In any load condition, tx_post and tx_complete should be
           converging.  tx_complete would follow tx_post monotonously,
           trying to keep up. 

           We detect anamoly in the above described behaviour. If the gap
           keeps increasing over a period of time(couple of seconds), we
           beleive the the HTC or target may have a failure. Upon detection
           of failure, we reset the target and HTC after which the 
           sub-system comes to initial state.

    How to invoke:
        checkTargetStatus <intf> <timer_value>
