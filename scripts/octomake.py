import sys, glob, time
import datetime as dt
from subprocess import Popen, DEVNULL

TIMEOUT = 60

devices = glob.glob('/dev/tty.SLAB_USBtoUART*')

args = ['make'] + sys.argv[1:]

children = [Popen(args + ['CONFIG_ESPTOOLPY_PORT=' + device], stdout=DEVNULL) for device in devices]

n_finished = 0
start = time.clock()
while n_finished < len(children) and time.clock() - start < TIMEOUT:

    status = [child.poll() for child in children]
    n_finished = sum([1 for s in status if s is not None])
    n_success = sum([1 for s in status if s == 0])
    n_failure = sum([1 for s in status if s != 0 and s is not None])

    print('{suc}/{tot} success    {fai}/{tot} failed'.format(
        suc=n_success, fai=n_failure, tot=len(children)))

    time.sleep(5)

# at this point, kill the remaining processes
for child in children:
    if child.poll() is None:
        child.kill()

if n_finished != len(children):
   print('Some process did not finish') 
