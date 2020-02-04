# Copyright 2020 Robin Scheibler
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
"""
This file defines the routine to read and write files
created with BlinkyViewer


The underlying format is [MessagePack](https://github.com/msgpack/msgpack-python)
which is suitable to efficiently save binary data.


The encoder/decoder were modelled after the
[msgpack-numpy package](https://github.com/lebedov/msgpack-numpy).
"""
import msgpack
import numpy as np
from datetime import datetime
from .version import __version__
from .utils import pixel_to_str


def encoder(obj, chain=None):
    """ Custom encoder to store numpy.ndarray in MessagePack format """

    if isinstance(obj, np.ndarray):

        # Make sure this is not a structured array type
        assert obj.dtype.kind != "V", "Unsupported non-numeric type"

        return {
            "__nd__": True,  # indicate this is a numpy ndarray
            "shape": obj.shape,
            "dtype": obj.dtype.str,
            "data": obj.tobytes(),
        }
    else:
        return obj if chain is None else chain(obj)


def decoder(obj, chain=None):
    """ Custom decoder to recover numpy.ndarray saved in MessagePack format """

    try:
        if "__nd__" in obj:
            return np.frombuffer(obj["data"], dtype=np.dtype(obj["dtype"])).reshape(
                obj["shape"]
            )
        else:
            return obj if chain is None else chain(obj)

    except KeyError:
        print("error!")
        # This is just a security in case an unrelated document
        # contains "__nd__" as a key
        return obj if chain is None else chain(obj)


class BlinkyFile(object):
    def __init__(self, locations, data, fps, version=None, creation=None, **metadata):
        self.locations = locations
        self.data = data

        assert len(self.locations) == self.data.shape[1], (
            "Error when creating the Blinky file object. "
            "The number of locations should correspond to the "
            "number of signals recorded."
        )

        self.fps = fps
        self.version = version if version is not None else __version__
        self.creation = (
            creation
            if creation is not None
            else datetime.now().astimezone().isoformat()
        )
        self.metadata = metadata

    def dump(self, filename):
        """ Saves the object as a MessagePack file """
        with open(filename, "wb") as f:
            msgpack.pack(self.__dict__, f, default=encoder, use_bin_type=True)

    @classmethod
    def load(cls, filename):
        """ Load a BlinkyFile object from MessagePack format """
        with open(filename, "rb") as f:
            content = msgpack.unpack(f, object_hook=decoder, raw=False)
        return cls(**content)


def file_preview():
    """
    Preview a Blinky file
    """
    import matplotlib.pyplot as plt
    import argparse

    parser = argparse.ArgumentParser(description="Preview a Blinky file")
    parser.add_argument("filename", type=str, help="File name")
    args = parser.parse_args()

    bfile = BlinkyFile.load(args.filename)
    data = bfile.data.astype(np.float)

    if len(data.shape) == 5:
        ## This is a color file, we will average the colors
        data = np.mean(data, axis=-1)

    # Now, for the purpose of preview, we average the boxes
    data = np.mean(data, axis=(-2, -1))

    # Make the time axis
    time = np.arange(data.shape[0]) / bfile.fps

    # Make the plot
    fig, ax = plt.subplots(1, 1)
    ax.plot(time, data)
    ax.set_xlabel("Time [s]")
    ax.set_ylabel("Pixel value")
    ax.set_title(f"File creation {bfile.creation}")
    ax.legend([pixel_to_str(p) for p in bfile.locations])

    plt.show()


if __name__ == "__main__":
    file_preview()
