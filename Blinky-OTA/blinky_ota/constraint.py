import os
import struct


def create_set(val):
    s = set()
    numbers = val.split(',')
    for number in numbers:
        if number.find('-') > -1:
            edges = number.split('-')
            if len(edges) >= 2:
                for i in range(int(edges[0]), int(edges[1])+1):
                    s.add(i)
        else:
            s.add(int(number))
    return s


# read
f = open('target.txt')
target = f.read()
f.close()

target_set = set()

lines = target.split('\n')
isAddLine = False
isRemoveLine = False
for line in lines:
    if line == 'add':
        isAddLine = True
    elif line == 'remove':
        isRemoveLine = True
    elif isAddLine:
        add_set = create_set(line)
        target_set |= add_set

        isAddLine = False
    elif isRemoveLine:
        remove_set = create_set(line)
        target_set -= remove_set

        isRemoveLine = False

print('target number is')
print(target_set)

# write
if not os.path.exists('build'):
    os.makedirs('build')
w = open('build/constraint.bin', 'wb')
for number in target_set:
    w.write(struct.pack('i', number))
w.close()

