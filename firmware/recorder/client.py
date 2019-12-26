# -*- coding:utf-8 -*-
from __future__ import print_function

from argparse import ArgumentParser
from socket import SO_REUSEADDR, SOCK_STREAM, error, socket, SOL_SOCKET, AF_INET
import os, errno, time, sys, traceback, wave, struct

parser = ArgumentParser(description="Client for Blinky recorder")
parser.add_argument(
    "-b",
    "--blinkies",
    default="",
    help="Specify recording devices and parameters, "
    "<ip>:<channels>:<samplerate>, e.g. 192.168.11.12:2:16000",
)
parser.add_argument(
    "-o", "--output", default="./", help="Specify output folder like. ./output"
)
args = parser.parse_args()


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


def download(v):
    address = v[0]
    port = v[1]
    chmode = v[2]
    samplerate = v[3]

    s = socket(AF_INET, SOCK_STREAM)

    try:
        s.settimeout(5)
        s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        s.connect((address, port))

        data = struct.pack("<i", chmode)
        result = s.send(data)
        data = struct.pack("<i", samplerate)
        result = s.send(data)

        if chmode == 0:
            chmodestr = "L"
        elif chmode == 1:
            chmodestr = "R"
        elif chmode == 2:
            chmodestr = "LR"
        print("chmode=", chmodestr, "samplerate=", samplerate)
        mkdir_p(args.output)
        ww = wave.open(
            "%s/%s_%s_%d_%s.wav"
            % (
                args.output,
                address,
                chmodestr,
                samplerate,
                time.strftime("%Y%m%d-%H%M%S"),
            ),
            "w",
        )
        chs = 1
        if chmode == 0 or chmode == 1:  # L or R only
            ww.setnchannels(1)
        else:  # LR
            ww.setnchannels(2)
            chs = 2
        ww.setsampwidth(2)  # 16bit
        ww.setframerate(samplerate)

        response = b""
        lastidx = -1
        while True:
            chunk = (1 + 512 * chs) * 2
            response += s.recv(chunk * 10)
            while len(response) >= chunk:
                idx = struct.unpack("<H", response[:2])[0]
                if (
                    lastidx != -1
                    and abs(idx - lastidx) != 1
                    and abs(idx - lastidx) != 65535
                ):
                    print(address, "buffer overflow", "idx=", idx, "lastidx=", lastidx)
                ww.writeframes(response[2:chunk])
                response = response[chunk:]
                lastidx = idx

        ww.close()
        s.close()
    except KeyboardInterrupt:
        return 0
    except error as e:
        print(e, address, port)
        traceback.print_exc()
        return -1
    except:
        traceback.print_exc()

    return 0


funcargs = []
for hostinfo in args.blinkies.split(","):
    address = hostinfo.split(":")[0]
    chs = int(hostinfo.split(":")[1])
    fs = int(hostinfo.split(":")[2])
    print(address, 8080, chs, fs)
    funcargs.append((address, 8080, chs, fs))

from multiprocessing import Pool

p = Pool(len(funcargs))

try:
    results = p.map(download, funcargs)
except KeyboardInterrupt:
    sys.stdout.write("\rfinish!")
    sys.exit()
