import sdcard, os, json, time, re, struct
from machine import SPI, Pin
import network, socket
import uhashlib, ubinascii

CONFIG_FILE = "config.json"

SD_ROOT = "/sd"
BIN_FILENAME = "/blinky.bin"  # <- firmware
HASH_FILENAME = "/hash.txt"
CONSTRAINT_FILENAME = "/constraint.bin"
TARGET_FILENAME = "/target.txt"

VALID_FILES = [BIN_FILENAME, HASH_FILENAME, CONSTRAINT_FILENAME]

MIME_TYPES = {
    "txt": "text/plain",
    "html": "text/html",
    "bin": "application/octet-stream",
}

MIME_FORMAT = {"txt": "r", "html": "r", "bin": "rb"}

RE_GET_FILENAME = "GET\s+([^ ]+)"

LED_BUILTIN_PIN = 5  # <- WEMOS D32 Pro
led = Pin(5, Pin.OUT)
led.off()

HTTP_404_NOT_FOUND = (
    "HTTP/1.1 404 Not Found\n"
    # "Date: Sun, 18 Oct 2012 10:36:20 GMT"
    "Server: microblinkyserver\n"
    "Connection: Closed\n"
)


def file_exists(fn):
    ''' tests if a file exists '''
    try:
        with open(fn, 'rb'):
            pass
        return True
    except:
        return False


def failure():
    while 1:
        led.on()
        time.sleep_ms(100)
        led.off()
        time.sleep_ms(100)


def compute_hash():

    binfile = SD_ROOT + BIN_FILENAME
    hashfile = SD_ROOT + HASH_FILENAME

    # if file does not exist, do nothing
    if not file_exists(binfile):
        return

    with open(binfile, 'rb') as f, open(hashfile, "wt") as h:
        sig = uhashlib.sha256()
        while True:
            data = f.read(1024)
            if len(data) == 0:
                break
            else:
                sig.update(data)

        hexdigest = ubinascii.hexlify(sig.digest()).decode()
        h.write(hexdigest)

    print("Hash computed:", hexdigest)


def create_binary_constraint_file():
    ''' Creates the binary target file from the target.txt file '''

    def parse_set(val):
        ''' Parse the lines selecting devices '''
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

    target_fn = SD_ROOT + TARGET_FILENAME
    constraint_fn = SD_ROOT + CONSTRAINT_FILENAME

    # if the target file do not exist, there should be
    # no constraint file either. Delete if it exists.
    # Then do nothing more.
    if not file_exists(target_fn):
        print("No target file, all blinkies are targets")
        if file_exists(constraint_fn):
            os.remove(constraint_fn)
        return

    # if the target file exists, let's parse it!
    with open(SD_ROOT + TARGET_FILENAME) as f:

        target_set = set()
        isAddLine = False
        isRemoveLine = False

        # read line-by-line
        for line in f:
            # remove white space around the line
            line = line.strip()

            if line == 'add':
                isAddLine = True

            elif line == 'remove':
                isRemoveLine = True

            elif isAddLine:
                add_set = parse_set(line)
                target_set |= add_set
                isAddLine = False

            elif isRemoveLine:
                remove_set = parse_set(line)
                target_set -= remove_set
                isRemoveLine = False

    print('Targets are:')
    print(target_set)

    # write the constraint file
    with open(constraint_fn, 'wb') as w:
        for number in target_set:
            w.write(struct.pack('i', number))


def start_access_point(config):
    """ Put the device in access point mode """
    ap = network.WLAN(network.AP_IF)  # create access-point interface
    ap.config(**config["access_point"]["config"])  # sets the ESSID and PW of the access point
    ap.ifconfig(config["access_point"]["ifconfig"])  # sets the IP, subnet, gateway, dns
    ap.active(True)  # activate the interface


def init_sd_card():

    # First we would like to bount the SD card
    try:
        spibus = SPI(1, sck=Pin(18), mosi=Pin(23), miso=Pin(19))
        sd = sdcard.SDCard(spibus, Pin(4))  # <- chipselect 4 on WEMOS D32 Pro
        os.mount(sd, SD_ROOT)
    except:
        failure()


if __name__ == "__main__":

    # let us read a config file from flash first
    with open(CONFIG_FILE, "r") as f:
        config = json.load(f)

    start_access_point(config)
    init_sd_card()
    compute_hash()
    create_binary_constraint_file()

    # Start to listen on a port
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("", config["server"]["port"]))
    s.listen(5)  # <- at most 5 connections

    while True:
        conn, addr = s.accept()
        print("Got a connection from %s" % str(addr))
        request = conn.recv(1024)
        request_str = request.decode()
        print("Content = %s" % request_str)
        print()

        # identify the file requested
        match = re.match(RE_GET_FILENAME, request_str)
        send_404 = False
        if match:
            # We found a filename, let's process this
            file_req = match.group(1)
            full_path = SD_ROOT + file_req
            print("File {} requested".format(file_req))

            if file_req in VALID_FILES and file_exists(full_path):
                print("Judged the request valid, trying to send file")

                ext = file_req.split(".")[-1]

                response = "hello world"
                conn.sendall("HTTP/1.1 200 OK\r\n")
                conn.sendall("Content-Type: {}\r\n".format(MIME_TYPES[ext]))
                conn.sendall("Connection: close\r\n\r\n")

                # send the file
                with open(full_path, MIME_FORMAT[ext]) as f:
                    while True:
                        data = f.read(1024)
                        if len(data) == 0:
                            break
                        else:
                            conn.sendall(data)

                conn.close()

                print("Finished sending. Connection closed.")

            else:
                send_404 = True

        else:
            print("No filename match")
            send_404 = True

        if send_404:
            # That's a 404
            conn.sendall(HTTP_404_NOT_FOUND)
            conn.close()
