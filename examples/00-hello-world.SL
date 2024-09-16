load "00-include.SL"

def _start
        echo Hello, world!
        echo nowhere   (hello)
        echo a b c
        xor %rdi, %rdi
        call _exit
def _exit
        exit
