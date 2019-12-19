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


def encoder(obj, chain=None):
    """ Custom encoder to store numpy.ndarray in messagepack format """

    if isinstance(obj, np.ndarray):

        # Make sure this is not a structured array type
        assert obj.dtype.kind != "V"

        return {
            "__nd__": True,  # indicate this is a numpy ndarray
            "shape": obj.shape,
            "dtype": obj.dtype.str,
            "data": obj.tobytes(),
        }
    else:
        return obj if chain is None else chain(obj)


def decoder(obj, chain=None):

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
            obj = cls(**content)
        return obj
