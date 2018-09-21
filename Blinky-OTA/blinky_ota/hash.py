import hashlib

with open("build/blinky.bin", "rb") as f:
    data = f.read()

w = open('build/hash.txt', 'w')
w.write(hashlib.md5(data).hexdigest())
w.close()
